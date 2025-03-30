//
// Created by antonio-filipovic on 3/29/25.
//

// check what does this exactly mean
#pragma once


namespace global {

    class SmartPointer{

    public:
        explicit SmartPointer(int *ptr);

        ~SmartPointer();

    private:
        // pointee, we are the owners now
        int *pointee_;

    };


}

