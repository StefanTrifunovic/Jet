
#ifndef _VMSTACK
#define _VMSTACK

#include "JetExceptions.h"

namespace Jet
{
	template<class T>
	class VMStack
	{
		unsigned int _size;
		
	public:
		T mem[5000];
		VMStack()
		{
			_size = 0;
			//mem = new T[size];
		}

		VMStack(unsigned int size)
		{
			_size = 0;
			//mem = new T[size];
		}

		VMStack<T> Copy()
		{
			VMStack<T> ns;
			for (unsigned int i = 0; i < this->_size; i++)
			{
				ns.mem[i] = this->mem[i];
			}
			ns._size = this->_size;
			return std::move(ns);
		}

		~VMStack()
		{
			//delete[] this->mem;
		}

		T Pop()
		{
			if (_size == 0)
				throw RuntimeException("Tried to pop empty stack!");
			return mem[--_size];
		}

		void QuickPop(int times = 1)
		{
			if (this->_size < times)
				throw RuntimeException("Tried to pop empty stack!");
			_size -= times;
		}

		T& operator[](unsigned int pos)
		{
			if (pos >= this->_size)
				throw RuntimeException("Bad Stack Index");

			return mem[pos];
		}

		T Peek()
		{
			return mem[_size-1];
		}

		void Push(T item)
		{
			if (_size >= 5000)
				throw RuntimeException("Stack overflow");

			mem[_size++] = item;
		}

		unsigned int size()
		{
			return _size;
		}
	};
}
#endif