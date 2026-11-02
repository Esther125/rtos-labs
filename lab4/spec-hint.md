Print
Warning that it is possible to print wrong values in ISR, though the value is actually correct. Thus, we suggest only using printf at main.c and setting breakpoint for debugging.
To show the needed message for demo, you can maintain a buffer to collect the information from task.c and print at main.c.
You can set a time limit to stop running and then print the information.
Task creation
You may adjust the order of the creation of tasks to decide which task to run first since the system may automatically run the task which is lastly created.
