idf_component_register(
    SRCS "data_sender.c"
    INCLUDE_DIRS "include"
    REQUIRES cfg_if net_if lwip data_parser
)
