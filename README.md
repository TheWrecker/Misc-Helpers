# Misc-Helpers

Standalone code snippets with various functions

## Executor
Simple threadpool with basic functionality, supports submission of orders.

* Integration:
Simply copy the associated .h file

* Usage: 

```
#include <memory>
#include <iostream>
#include "Executor.h"

bool IsGreater(int i1, int i2)
{
    return i1 > i2;
}

void main()
{
    auto executor = std::make_unique<PDL::Executor>();
    auto future = executor->Execute(IsGreater, 2, 1);

    if (future.get())
        std::cout << "True" << std::endl;
    else
        std::cout << "False" << std::endl;
        
}

```

