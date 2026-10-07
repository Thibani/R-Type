# Each module is its own library target. Dependencies flow downward only:
#   client / server  ->  game  ->  engine  ->  ecs

add_subdirectory(ecs)
