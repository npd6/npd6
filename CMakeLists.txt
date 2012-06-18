cmake_minimum_required(VERSION 2.6)

project(npd6)

add_executable(npd6 main.c icmp6.c util.c ip6.c config.c)

install(FILES man/npd6.man DESTINATION share/man/man8 RENAME npd6.8)
install(FILES man/npd6.conf.man DESTINATION share/man/man5 RENAME npd6.conf.5)
install(FILES etc/npd6.conf DESTINATION /etc RENAME npd6.conf.sample)
install(FILES etc/npd6 DESTINATION /etc/init.d)

