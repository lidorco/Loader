# Loader
Load a dll file received from the network, enabling to run the dll exports functions. 

## Why I made it?
The main goal of this project is for an educational purpose, and to practice cpp and windows.

### Prerequisites
* Visual Studio 2015
* Boost 1.65.0
* Python

## Compile and Deployment:
Currently, this project load only dll on x86 process. So you need to make sure that the solution platform is x86.
* Build the project (Ctrl+Shift+B)
* Run it (Ctrl+F5)

## Future work
I Would like to extend this project so it can load other types of PE files such as exe file: it would create 'empty' process and read exe file, 'filling' the process with all the binary and run the process. Adding optional arguments for the way the process should be created.

## Contributing
Any contribution is welcome =]

## Futher reading
https://www.joachim-bauch.de/tutorials/loading-a-dll-from-memory/
https://www.codeproject.com/Articles/12532/Inject-your-code-to-a-Portable-Executable-file
https://www.codeproject.com/Tips/139349/Getting-the-address-of-a-function-in-a-DLL-loaded
