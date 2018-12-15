# RolLang

This is my programming language project. I am trying to make a language similar to C#, but 
with some modifications, including:

* Separate the loading stage. Loading stage should produce something easier for the backend 
to handle.
* Stronger generic system. Try to be close to a simplified C++. More flexible constrains. 
Generic typedefs. Duck typing. Parameter pack.
* Allow one to easily write GC-free (though may be unsafe) code.
* No reflection, but may allow some code to execute and collect data at loading stage.

## Architecture

* Frontend implementation.
* IR builder.
* ---- Byte-code intermediate representation ----
* IR loader.
* Backend implementation. 

Currently there is no compiler frontend. I plan to start it after the basic functions of 
loader is finished. The IR builder can be modified from the one being used by the tests. 
For the backend, only an interpreter is planned and started.

## Roadmap
[TodoList](https://github.com/RolLang/RolLangTodoList)
