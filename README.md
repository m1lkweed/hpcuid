No UID scheme is suitable for all use-cases. With that in mind, the following is a best-effort attempt at a UID generation scheme for HPC environments.
HPCUID is a proposal for easily-constructable unique identifiers designed to match the needs and abilities of high-performance compute clusters. These UIDs can act as snowflakes, database keys, or contention tie-breakers.
No values rely on randomness nor on IPC to be generated, and all values are guaranteed to be unique for a given cluster.
As this is intended for internal use in homogeneous systems, no byte-ordering is preferred for individual elements. An implementation may choose any byte ordering to improve speed or to conform with other specifications.

The basic structure of an HPCUID is as follows:
1. The most-significant byte is reserved and must be set to all zeros.
2. The next 3 bytes are the lower three bytes (NIC) of the hardware MAC address of the node or device.
3. The next 4 bytes are the OS-given PID of the current thread/process. This, along with the previous two entries, should be constant for all generated HPCUIDs throughout the lifetime of a thread.
4. The next 4 bytes are an unsigned timestamp representing the number of seconds elapsed since 2001-01-01T00:00Z.
5. The next and final 4 bytes are an unsigned monotonic counter which must update every time an HPCUID is generated and resets to 0 then the timestamp is incremented.

On compilers for the x86 family, the following C struct implements the basic layout of an HPCUID:
```c
typedef struct{
	uint32_t increment;
	uint32_t timestamp;
	uint32_t processid;
	uint32_t machineid:24;
	uint32_t _reserved:8;
}hpcuid_t;
```
Care should be taken to make sure process IDs are not shared between CPU cores and any accelerators on the same machine.
Process IDs MUST be unique. For development purposes, a non-clustered machine may use a machine ID of `0x00'00'00`, making the first 32 bits all zeros.
It is expected that any machine capable of being in a cluster has timekeeping and multithreading capabilities, so the process ID and timestamp must still be calculated.

This system has a number of benefits compared to other UIDs:
* Non-configured: Unlike Snowflake UUIDs a central server is not required and no provisioning must be done.
* Globally *k*-sortable and locally time-sorted per-worker, unlike UUIDs.
* Unicity guaranteed with over 4 billion (32 bits) unique ids per second from each worker, 4 billion workers per machine, and over to 16 million (24 bits) machines in a single cluster.
* Lock-free (unlike UUIDs v1 and v2)
* GPU-friendly and future-proof: An RTX 4090 only has 16,384 cores--1/65536th of the process ID space--and a clock speed of 2.5 GHz--less than half of what the increment field can support per second.

For completeness, the following datatype can make HPCUIDs easier to work with (x86 family):
```c
typedef union{
	unsigned __int128 as_u128;
	struct{
		union{
			uint64_t discriminators;
			struct hpcuid_discriminators{
				uint32_t increment;
				uint32_t timestamp;
			}hpcuid_discriminators;
		};
		union{
			uint64_t globals;
			struct hpcuid_globals{
				uint32_t processid;
				uint32_t machineid:24;
				uint32_t _reserved:8;
			}hpcuid_globals;
		};
	};
}hpcuid_t;
```
