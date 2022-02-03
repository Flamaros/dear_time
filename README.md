# Dear Time
Dear Time is an application that monitor processes to display a a chart and some statistics.

I develop it to try "Dear ImGui" and "ImPlot" libraries, because I really wanted to feel how it is to use a GUI library based on the immediate paradigm.
It was also an occasion to mesure how much time I wait after C++ toolchains to build applications I am working on at my day job.

A capture:
![image](https://user-images.githubusercontent.com/1679168/165382026-f91e2291-8810-41e6-b6e7-163d9106036a.png)

## How it works
You create a group of processes you want to monitor as if it was only one instance. The application merge execution time frames of all processes inside the group,
so if executions some processes is overlapped the computed duration for the group will correspond to the time between the start of the first process to the termination
of the last one. This is usefull to monitor durations of operations done by executing many processes like compiling a C/C++ program.

![image](https://user-images.githubusercontent.com/1679168/165383291-c3d02739-0193-4621-a2c2-ebe929cf549b.png)

## Build it
There is an alread compiled x64 binary in the bin folder. But if you really want to build it, you only need open "Dear Time.sln" with  Visual Studio 2019.

## Limitations
If a process is in many groups it will not work as expected, events of the process will be sent to only one group.
