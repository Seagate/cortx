add_custom_target(tools)

add_executable(lambda-pp EXCLUDE_FROM_ALL lambda-pp.c)

add_dependencies(tools lambda-pp)
