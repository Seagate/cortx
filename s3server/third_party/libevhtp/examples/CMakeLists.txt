add_custom_target(examples)

add_executable(test EXCLUDE_FROM_ALL test.c)
add_executable(test_basic EXCLUDE_FROM_ALL test_basic.c)
add_executable(test_vhost EXCLUDE_FROM_ALL test_vhost.c)
add_executable(test_client EXCLUDE_FROM_ALL test_client.c)
add_executable(test_query EXCLUDE_FROM_ALL test_query.c)
add_executable(test_perf EXCLUDE_FROM_ALL test_perf.c)

if (NOT EVHTP_DISABLE_EVTHR)
	add_executable(test_proxy EXCLUDE_FROM_ALL test_proxy.c)
	target_link_libraries(test_proxy evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
endif()

target_link_libraries(test        evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
target_link_libraries(test_basic  evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
target_link_libraries(test_vhost  evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
target_link_libraries(test_client evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
target_link_libraries(test_query  evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})
target_link_libraries(test_perf   evhtp ${LIBEVHTP_EXTERNAL_LIBS} ${SYS_LIBS})

add_dependencies(examples test test_basic test_vhost test_client test_proxy test_query test_perf)


