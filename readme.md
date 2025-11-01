# How to use
Just include the `Pilo.h` file in your C++ project and you are ready to go.

You can create a new "GPIO Controller" using GPIO<name_of_the_chip, your_program_label> gpio{};
The program_label is the string used to register your contoller.

from there you can use the object.
first you need to add the pins you want to access.
`gpio.add_lines<line_num, line_num, line_num>();` will register them for you.
`gpio.write<num>(bool_value)` will write to the selected pin.
