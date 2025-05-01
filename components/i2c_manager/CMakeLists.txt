idf_component_register(SRCS "i2c_master_control.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver common_definitions module_i2c_proto)