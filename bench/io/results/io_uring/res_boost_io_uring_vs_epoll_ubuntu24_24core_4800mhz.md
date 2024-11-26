**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: 12th Gen Intel(R) Core(TM) i7-12800HX 0.8-4.8GHz, 24 CPUs, 2 threads per core

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3500000
Run count: 3
Summary:
* 412 620 messages per second;
* x2.142 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         410 979
Message/sec(med) median:      412 620
Message/sec(med) max:         415 239

== Median report:

   Messages(total): 3511080
     Duration(sec): 8.512000
Message/sec(total): 412485
  Message/sec(med): 412620
  Message/sec(max): 416725
   Latency ms(med): 0.241492
   Latency ms(max): 0.246032

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         192 291
Message/sec(med) median:      192 639
Message/sec(med) max:         192 658

== Median report:

   Messages(total): 3507145
     Duration(sec): 18.227000
Message/sec(total): 192414
  Message/sec(med): 192639
  Message/sec(max): 196220
   Latency ms(med): 0.519755
   Latency ms(max): 0.545833
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3000000
Run count: 3
Summary:
* 182 537 messages per second;
* x0.948 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         181 614
Message/sec(med) median:      182 537
Message/sec(med) max:         183 527

== Median report:

   Messages(total): 3013499
     Duration(sec): 16.532000
Message/sec(total): 182282
  Message/sec(med): 182537
  Message/sec(max): 184850
   Latency ms(med): 0.548239
   Latency ms(max): 0.553285

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         192 449
Message/sec(med) median:      192 620
Message/sec(med) max:         192 859

== Median report:

   Messages(total): 3003794
     Duration(sec): 15.620000
Message/sec(total): 192304
  Message/sec(med): 192620
  Message/sec(max): 195250
   Latency ms(med): 0.518685
   Latency ms(max): 0.536429
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400 -message_target_count 200000
Run count: 3
Summary:
* 14 053 messages per second;
* x1.591 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:          13 646
Message/sec(med) median:       14 053
Message/sec(med) max:          14 324

== Median report:

   Messages(total): 201363
     Duration(sec): 14.421000
Message/sec(total): 13963
  Message/sec(med): 14053
  Message/sec(max): 14941
   Latency ms(med): 14.246723
   Latency ms(max): 24.490296

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:           8 792
Message/sec(med) median:        8 833
Message/sec(med) max:           9 005

== Median report:

   Messages(total): 200068
     Duration(sec): 22.731000
Message/sec(total): 8801
  Message/sec(med): 8833
  Message/sec(max): 9107
   Latency ms(med): 22.579250
   Latency ms(max): 23.900054
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 1 936 645 messages per second;
* x2.104 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:       1 909 980
Message/sec(med) median:    1 936 645
Message/sec(med) max:       1 958 980

== Median report:

   Messages(total): 20153310
     Duration(sec): 10.420000
Message/sec(total): 1934098
  Message/sec(med): 1936645
  Message/sec(max): 1953482
   Latency ms(med): 0.258135
   Latency ms(max): 0.264011

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         907 976
Message/sec(med) median:      920 362
Message/sec(med) max:         921 450

== Median report:

   Messages(total): 20050755
     Duration(sec): 21.830000
Message/sec(total): 918495
  Message/sec(med): 920362
  Message/sec(max): 934972
   Latency ms(med): 0.544026
   Latency ms(max): 0.580863
```
</details>
