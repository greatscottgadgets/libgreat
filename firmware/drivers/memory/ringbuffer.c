#include <string.h>
#include <errno.h>

#include <debug.h>

#include <drivers/memory/ringbuffer.h>


void ringbuffer_init(ringbuffer_t *rb, void *buffer, size_t size)
{
    rb->size   = size;
    rb->buffer = buffer;

    // Clear out the ringbuffer...
    memset(rb->buffer, 0, rb->size);

    //  ... and reset our metadata.
    rb->write_index = 0;
    rb->read_index = 0;
}

int ringbuffer_enqueue(ringbuffer_t *rb, uint8_t element)
{
    uint64_t write_index;

    if (ringbuffer_full(rb)) {
        pr_debug("ringbuffer: error: tried to enqueue when ringbuffer is full!\n");
        return ENOMEM;
    }

    write_index = rb->write_index++;
    rb->buffer[write_index % rb->size] = element;

    return 0;
}


int ringbuffer_enqueue_overwrite(ringbuffer_t *rb, uint8_t element)
{
    // If we're full, dequeue the last element, so we can overwrite its spot.
    if (ringbuffer_full(rb)) {
        ringbuffer_dequeue(rb);
    }

    return ringbuffer_enqueue(rb, element);
}


int ringbuffer_dequeue(ringbuffer_t *rb)
{
    int element;

    if (ringbuffer_empty(rb)) {
        pr_debug("ringbuffer: error: tried to dequeue when ringbuffer is empty!\n");
        return -1;
    }

    element = rb->buffer[rb->read_index % rb->size];
    rb->read_index++;

    return element;
}

// it seems like there are two options for determining fullness/emptiness:
// 1. Keeping a "count" that increments/decrements for each enqueue/dequeue call
// 2. Not allowing data to be overwritten: if write index is one behind the read index
// then the buffer is full and we won't allow anymore writes to happen, thus preventing
// the write index to increment again, becoming equal to the read index; and we can
// use r/w index equality to determine emptiness

uint32_t ringbuffer_data_available(ringbuffer_t *rb)
{
	return rb->write_index - rb->read_index;
}

bool ringbuffer_full(ringbuffer_t *rb)
{
	return ringbuffer_data_available(rb) >= rb->size;
}

bool ringbuffer_empty(ringbuffer_t *rb)
{
	return ringbuffer_data_available(rb) == 0;
}
