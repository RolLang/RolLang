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

- [x] Serialization/deserialization
- [x] Loader
	- [x] Load generic types
		- [x] Value types
		- [x] Reference types
		- [x] Interface types
		- [x] Global (static) types
	- [x] Load generic functions
	- [x] Load native types
	- [x] Load special types
		- [x] Pointer
		- [x] Box
		- [x] Reference
		- [x] Embed
	- [x] Load RefList
		- [x] Referenced types
		- [x] Referenced functions
		- [x] Referenced fields
	- [x] Load subtype (type alias)
		- [x] Generic subtype
	- [x] Load type handlers
	- [x] Export/import
		- [x] Types
		- [x] Functions
		- [x] Constants
		- [ ] Field references (testing)
		- [ ] Traits (testing)
		- [ ] Redirect (testing)
	- [x] Inheritance
		- [x] Virtual functions
		- [x] Base class
		- [x] Interface
	- [x] Generic constrain
		- [x] Constrain types
			- [x] Type exist
			- [x] Type same
			- [x] Type inheritance
			- [x] Trait
		- [x] Trait member
			- [x] Sub-constrains
			- [x] Fields
			- [x] Functions
		- [x] Constrain member reference
			- [x] Types
			- [ ] Functions (testing)
			- [ ] Virtual functions (testing)
			- [ ] Fields (testing)
		- [ ] Parameter pack
			- [ ] Argument list segment support
			- [ ] Variable size segment
			- [ ] Reference list expanding support
			- [ ] Reference list expanding level control
			- [ ] Constrain type deduction
			- [ ] Parameter
			- [ ] Field
		- [ ] Partial specialization (type overload)
	- [ ] Variable-sized object
	- [ ] Attribute
- [ ] Interpreter
	- [ ] Redesign member instructions
		- Function call
		- Field
	- [ ] Rewrite stack
