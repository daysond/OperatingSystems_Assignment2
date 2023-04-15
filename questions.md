# Questions

## Is it necessary to synchronize the transducer and the data center acquisition unit with both semaphores as well as a status byte?

In this case, not really. Only one of them would suffice given data acquisition is reading the status and the transducer is modifying. And MemStatus is an enum, meaning the statuses are integer. Changing the value of an integer type is atomic. Therefore, the status will not be partially changed when data acquisition is trying to read the status while the transducer is modifying it.

## How big is the shared memory in bytes?

```cpp

struct SeismicData {
    MemStatus      status;          //  4 Bytes
    unsigned short packetLen;       //  2 Bytes
    char           data[BUF_LEN];   //  256 Bytes, BUF_LEN is 256
};                                  //  262 Bytes

struct SeismicMemory {
    unsigned int   packetNo;                    //  4 Bytes
    struct SeismicData seismicData[NUM_DATA];   //  262 Bytes * NUM_DATA, NUM_DATA is 2096
};                                              //  549,156 Bytes 
```

Therefore, the size of the shared memory is 549,156 Bytes, about 0.55MB. 

## When writing operating systems code, do you prefer to use object oriented programming with classes or do you prefer using C++ as an extended C with global variables inside the CPP file? Why?

It depends. If the problem can really fit into a class (or actually maps to an object in real life) and the problem is complex, it's better to write it using OOP and the code would be easier to maintain since the related data are organized and the functionalities are modularized. But for small problems, using C++ as an extended C with global variables is fine because we can still organize the code using functions.

## For this project, what are the advantages and disadvantages of using datagrams for our network communications?

Disadvantage: not secure, vulnerable to DDoS attacks, data integrity not guaranteed

Advantage: broadcasts data fast, no connection setup required

## How would you resolve a situation where a valid client ended up on the rogue list?

In general, we would want to remove the client from the rogue list. In order to verify if the client is indeed valid, we could check system logs. Furthermore, we can also change the criteria used to identify rogue clients and implement temporary rogue lists and a permanent rogue list. The duration of the block of the temporary list will increase over time as the client continues to exhibit the same behaviour.  I remember I was scraping data from a website and I did not use sleep(), my IP was then blocked by their server for a few minutes.

## Should the data passing between the data acquisition unit and the data centers be encrypted? Why?

Whether the data should be encrypted depends on the sensitivity of the data and the associated risk and the security needs. In this case, security is not really a concern when the data acquisition passes the data to the data centers. Therefore, the data can be sent as is to avoid unnecessary overhead. However, the data that data centers send to the data acquisition unit contains sensitive information, such as passwords. Therefore, this data needs to be encrypted.

