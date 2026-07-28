@ cdecl mapped_event_circular_buffer_create_shm(ptr int64)
@ cdecl mapped_event_circular_buffer_open_shm(ptr int64)
@ cdecl mapped_event_circular_buffer_close_shm(ptr)

@ cdecl mapped_event_circular_buffer_create()
@ cdecl mapped_event_circular_buffer_destroy(ptr)

@ cdecl mapped_event_circular_buffer_write(ptr ptr int64)
@ cdecl mapped_event_circular_buffer_read(ptr ptr ptr)

@ cdecl mapped_event_circular_buffer_wait_for_data(ptr)

@ cdecl shared_memory_block_create_shm(ptr int64)
@ cdecl shared_memory_block_open_shm(ptr int64)
@ cdecl shared_memory_block_close_shm(ptr)
@ cdecl shared_memory_block_destroy(ptr)
@ cdecl shared_memory_block_get_pointer(ptr)