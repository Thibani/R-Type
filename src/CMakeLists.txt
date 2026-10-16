# Each module is its own library target. Dependencies flow downward only:
#   client / server  ->  game  ->  engine  ->  ecs
#   client / server  ->  network
# ecs and network are independent of each other.

add_subdirectory(ecs)
add_subdirectory(network)
add_subdirectory(server)
