/*
 * Copyright (c) 2023, mr-library Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-09-03     MacRsh       first version
 */

#include "mrapi.h"

#if (MR_CFG_TASK == MR_CFG_ENABLE)

#define DEBUG_TAG   "task"

static mr_err_t err_io_task_cb(mr_task_t cb, void *args)
{
    return -MR_ERR_IO;
}

/**
 * @brief This function finds a task.
 *
 * @param name The name of the task.
 *
 * @return A pointer to the found task, or MR_NULL if not found.
 */
mr_task_t mr_task_find(const char *name)
{
    MR_ASSERT(name != MR_NULL);

    /* Find the task object from the container */
    return (mr_task_t)mr_object_find(name, Mr_Object_Type_Task);
}

/**
 * @brief This function adds a task to the container.
 *
 * @param task The task to be added.
 * @param name The name of the task.
 * @param table The table of the task.
 * @param table_size The size of the table.
 * @param queue_size The size of the queue.
 *
 * @note If tasks are lost, increase the queue size or processing frequency.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_task_add(mr_task_t task,
                     const char *name,
                     mr_task_table_t table,
                     mr_size_t table_size,
                     mr_size_t queue_size)
{
    mr_uint8_t *memory = MR_NULL;
    mr_size_t count = 0;
    mr_err_t ret = MR_ERR_OK;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(name != MR_NULL);
    MR_ASSERT(table != MR_NULL);
    MR_ASSERT(table_size != 0 && table_size <= MR_UINT8_MAX);

    /* Allocate memory for the task queue */
    memory = mr_malloc(queue_size * sizeof(mr_uint16_t));
    if (memory == MR_NULL)
    {
        MR_DEBUG_E(DEBUG_TAG, "[%s] add failed: [%d]\r\n", name, -MR_ERR_NO_MEMORY);
        return -MR_ERR_NO_MEMORY;
    }

    /* Initialize the private fields */
    task->active = MR_DISABLE;
    task->sm_active = MR_DISABLE;
    task->usage_max = 0;
    task->event = 0;
    task->sm = 0;
    task->tick = 0;
    mr_rb_init(&task->queue, memory, queue_size * sizeof(mr_uint16_t));
    mr_list_init(&task->run);
    task->table = table;
    task->table_size = table_size;

    /* Protect the callback function of each task */
    for (count = 0; count < task->table_size; count++)
    {
        task->table[count].cb = task->table[count].cb ? task->table[count].cb : err_io_task_cb;
        mr_list_init(&task->table[count].timing.run);
    }

    /* Add the object to the container */
    ret = mr_object_add(&task->object, name, Mr_Object_Type_Task);
    if (ret != MR_ERR_OK)
    {
        MR_DEBUG_E(DEBUG_TAG, "[%s] add failed: [%d]\r\n", name, ret);
        mr_free(memory);
    }

    return ret;
}

/**
 * @brief This function removes a task from the container.
 *
 * @param task The task to be removed.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_task_remove(mr_task_t task)
{
    mr_err_t ret = MR_ERR_OK;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    /* Remove the object from the container */
    ret = mr_object_remove(&task->object);
    if (ret != MR_ERR_OK)
    {
        MR_DEBUG_E(DEBUG_TAG, "[%s] remove failed: [%d]\r\n", task->object.name, ret);
        return ret;
    }

    /* Reset the private fields */
    task->active = MR_DISABLE;
    task->sm_active = MR_DISABLE;
    task->usage_max = 0;
    task->event = 0;
    task->sm = 0;
    task->tick = 0;
    mr_free(task->queue.buffer);
    mr_rb_init(&task->queue, MR_NULL, 0);
    mr_list_init(&task->run);
    task->table = MR_NULL;
    task->table_size = 0;

    return MR_ERR_OK;
}

/**
 * @brief This function updates the tick of a task.
 *
 * @param task The task to be updated.
 * @param tick The tick of the task.
 */
void mr_task_update_tick(mr_task_t task, mr_uint32_t tick)
{
    mr_uint32_t curr_tick = 0;
    mr_list_t list = MR_NULL;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);
    MR_ASSERT(tick != 0 && tick < MR_UINT32_MAX / 2);

    /* Check whether the task is active */
    if (task->active == MR_DISABLE)
    {
        return;
    }

    /* Update the current tick and save it */
    curr_tick = task->tick += tick;

    for (list = task->run.next; list != &task->run; list = list->next)
    {
        mr_task_table_t timeout_task = mr_container_of(list, struct mr_task_table, timing.run);

        /* Check whether the current tick is larger than the timeout */
        if ((curr_tick - timeout_task->timing.timeout) >= MR_UINT32_MAX / 2)
        {
            break;
        }

        /* Remove a timeout task */
        list = list->prev;
        mr_list_remove(&timeout_task->timing.run);

        /* Get the index of the timeout task */
        mr_uint32_t index = ((mr_uint32_t)timeout_task - (mr_uint32_t)task->table) / sizeof(struct mr_task_table);

        /* Publish the timeout event */
        mr_task_post_event(task, index, MR_TASK_EVENT_TIMING);

        /* Restart the timeout task, if it is periodic */
        mr_task_timing(task, index, task->table[index].timing.interval, MR_TASK_TIMING_FLAG_PERIODIC);
    }
}

/**
 * @brief This function handles the events of a task.
 *
 * @param task The task to be handled.
 */
void mr_task_handle(mr_task_t task)
{
    mr_uint16_t index_and_label = 0;
    mr_uint8_t index = 0;
    mr_size_t count = 0;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    /* Check whether the task is active */
    if (task->active == MR_DISABLE)
    {
        return;
    }

    /* Get the number of events */
    count = mr_rb_get_data_size(&task->queue);

    while (count != 0)
    {
        /* Get the index and event from the queue */
        count -= mr_rb_read(&task->queue, &index_and_label, sizeof(index_and_label));

        /* Get the index and event */
        index = index_and_label >> 8;
        task->event = index_and_label & 0xFF;

        /* Call the task callback function */
        task->table[index].cb(task, task->table[index].args);
    }

    /* Check whether the state machine is active */
    if (task->sm_active == MR_ENABLE)
    {
        task->event = MR_TASK_EVENT_SM;
        task->table[task->sm].cb(task, task->table[task->sm].args);
    }
}

/**
 * @brief This function starts a task.
 *
 * @param task The task to be started.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_task_start(mr_task_t task)
{
    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    task->active = MR_ENABLE;

    return MR_ERR_OK;
}

/**
 * @brief This function stops a task.
 *
 * @param task The task to be stopped.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_task_stop(mr_task_t task)
{
    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    task->active = MR_DISABLE;

    return MR_ERR_OK;
}

/**
 * @brief This function posts an event to a task.
 *
 * @param task The task to be posted.
 * @param index The index of the task table.
 * @param event The event to be posted.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 *
 * @note User event ranges from 0 to 251, build-in event ranges from 252 to 255.
 */
mr_err_t mr_task_post_event(mr_task_t task, mr_uint8_t index, mr_uint8_t event)
{
    mr_uint16_t index_and_event = 0;
    mr_size_t count = 0;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    /* Check whether the index is valid */
    if (index >= task->table_size)
    {
        MR_DEBUG_E(DEBUG_TAG,
                   "[%s -> %d] post [%d] failed: [%d]\r\n",
                   task->object.name,
                   index,
                   event,
                   -MR_ERR_INVALID);
        return -MR_ERR_INVALID;
    }

    /* Check whether the task is active */
    if (task->active == MR_DISABLE)
    {
        MR_DEBUG_E(DEBUG_TAG,
                   "[%s -> %d] post [%d] failed: [%d]\r\n",
                   task->object.name,
                   index,
                   event,
                   -MR_ERR_GENERIC);
        return -MR_ERR_GENERIC;
    }

    /* Save the index and event */
    index_and_event = (index << 8) | event;

    /* Publish the index and event */
    count = mr_rb_write(&task->queue, &index_and_event, sizeof(index_and_event));
    if (count != sizeof(index_and_event))
    {
        MR_DEBUG_E(DEBUG_TAG,
                   "[%s -> %d] post [%d] failed: [%d]\r\n",
                   task->object.name,
                   index,
                   event,
                   -MR_ERR_BUSY);
        return -MR_ERR_BUSY;
    }

#if (MR_CFG_TASK_USAGE == MR_ENABLE)
    task->usage_max = mr_max(task->usage_max, mr_task_get_usage(task));
#endif

    return MR_ERR_OK;
}

/**
 * @brief This function starts a task timing.
 *
 * @param task The task to be started.
 * @param index The index of the task table.
 * @param tick The time for timing.
 * @param flag The flag for timing, one-shot or periodic.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 *
 * @note If want to stop the timing, set tick to 0.
 */
mr_err_t mr_task_timing(mr_task_t task, mr_uint8_t index, mr_uint32_t tick, mr_uint8_t flag)
{
    mr_list_t list = MR_NULL;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);
    MR_ASSERT(flag == MR_TASK_TIMING_FLAG_ONESHOT || flag == MR_TASK_TIMING_FLAG_PERIODIC);

    /* Check whether the index is valid */
    if (index >= task->table_size)
    {
        MR_DEBUG_E(DEBUG_TAG,
                   "[%s -> %d] timing [%d] failed: [%d]\r\n",
                   task->object.name,
                   index,
                   tick,
                   -MR_ERR_INVALID);
        return -MR_ERR_INVALID;
    }

    /* Check whether the task is active */
    if (task->active == MR_DISABLE)
    {
        MR_DEBUG_E(DEBUG_TAG,
                   "[%s -> %d] timing [%d] failed: [%d]\r\n",
                   task->object.name,
                   index,
                   tick,
                   -MR_ERR_GENERIC);
        return -MR_ERR_GENERIC;
    }

    /* Remove the task, if it is running */
    if (mr_list_is_empty(&task->table[index].timing.run) != MR_TRUE)
    {
        /* Disable interrupt */
        mr_interrupt_disable();

        mr_list_remove(&task->table[index].timing.run);

        /* Enable interrupt */
        mr_interrupt_enable();
    }

    /* Stop the task */
    if (tick == 0)
    {
        return MR_ERR_OK;
    }

    /* Disable interrupt */
    mr_interrupt_disable();

    /* Set the interval */
    if (flag == MR_TASK_TIMING_FLAG_ONESHOT)
    {
        task->table[index].timing.interval = 0;
    } else
    {
        task->table[index].timing.interval = tick;
    }

    /* Set the timeout */
    task->table[index].timing.timeout = task->tick + tick;

    /* Insert the task into the list */
    for (list = task->run.next; list != &task->run; list = list->next)
    {
        mr_task_table_t be_insert_task = mr_container_of(list, struct mr_task_table, timing.run);

        /* Compare the timeout */
        if (task->table[index].timing.timeout < be_insert_task->timing.timeout)
        {
            mr_list_insert_before(&be_insert_task->timing.run, &task->table[index].timing.run);
            break;
        }
    }
    if (mr_list_is_empty(&task->table[index].timing.run) == MR_TRUE)
    {
        mr_list_insert_before(&task->run, &task->table[index].timing.run);
    }

    /* Enable interrupt */
    mr_interrupt_enable();

    return MR_ERR_OK;
}

/**
 * @brief This function transitions state of a task.
 *
 * @param task The task to be transitioned.
 * @param index The index of the task table.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_task_transition_state(mr_task_t task, mr_uint8_t index)
{
    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    /* Check whether the index is valid */
    if (index >= task->table_size)
    {
        MR_DEBUG_E(DEBUG_TAG, "[%s] transition [%d] failed: [%d]\r\n", task->object.name, index, -MR_ERR_INVALID);
        return -MR_ERR_INVALID;
    }

    /* Check whether the task is active */
    if (task->active == MR_DISABLE)
    {
        MR_DEBUG_E(DEBUG_TAG, "[%s] transition [%d] failed: [%d]\r\n", task->object.name, index, -MR_ERR_GENERIC);
        return -MR_ERR_GENERIC;
    }

    /* Publish the exit event */
    if (task->sm_active == MR_ENABLE)
    {
        mr_task_post_event(task, task->sm, MR_TASK_EVENT_SM_EXIT);
    }

    /* Transition to the new state */
    task->sm = index;

    /* Publish the enter event, and enable the state machine */
    mr_task_post_event(task, task->sm, MR_TASK_EVENT_SM_ENTER);
    task->sm_active = MR_ENABLE;

    return MR_ERR_OK;
}

/**
 * @brief This function gets the event of a task.
 *
 * @param task The task to be queried.
 *
 * @return The event of the task.
 */
mr_uint8_t mr_task_get_event(mr_task_t task)
{
    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    return task->event;
}

/**
 * @brief This function gets the tick of a task.
 *
 * @param task The task to be queried.
 *
 * @return The tick of the task.
 */
mr_uint32_t mr_task_get_tick(mr_task_t task)
{
    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    return task->tick;
}

/**
 * @brief This function gets the usage rate of a task.
 *
 * @param task The task to be queried.
 *
 * @return The usage rate(0 ~ 100) of the task.
 */
mr_uint8_t mr_task_get_usage(mr_task_t task)
{
    mr_size_t usage = 0;
    mr_size_t total = 0;

    MR_ASSERT(task != MR_NULL);
    MR_ASSERT(task->object.type == Mr_Object_Type_Task);

    usage = mr_rb_get_data_size(&task->queue);
    total = mr_rb_get_buffer_size(&task->queue);

    return usage * 100 / total;
}

#endif