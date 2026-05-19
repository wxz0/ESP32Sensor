idf_component_register(
    SRCS "PressureSensor.c" "PressureSensorProtocol.c"
    INCLUDE_DIRS "include"
    REQUIRES bt Common
)
