@ cdecl mapped_event_circular_buffer_create_shm(ptr int64)
@ cdecl mapped_event_circular_buffer_open_shm(ptr int64)
@ cdecl mapped_event_circular_buffer_close_shm(ptr)

@ cdecl mapped_event_circular_buffer_create()
@ cdecl mapped_event_circular_buffer_destroy(ptr)

@ cdecl mapped_event_circular_buffer_write(ptr ptr int64)
@ cdecl mapped_event_circular_buffer_read(ptr ptr ptr)

@ cdecl mapped_event_circular_buffer_wait_for_data(ptr)