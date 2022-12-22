add_executable(telnetd telnet-core.c telnetd.c)
target_link_libraries(telnetd
    libtelnet
)
