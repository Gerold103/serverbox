**OS**: Debian GNU/Linux 11 (bullseye)<br>
**CPU**: AMD EPYC 7502P 32-Core Processor 2.5GHz, 32 CPUs, 2 threads per core

---
**Basic**

```
Command line args: -thread_count 10 -connect_count_per_port 1000 -message_payload_size 128 -message_int_count 5 -message_target_count 5000000
Run count: 5
Summary:
* 609 007 messages per second;
* x1.798 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         607 709
Message/sec(med) median:      609 007
Message/sec(med) max:         611 478

== Median report:

   Messages(total): 5060876
     Duration(sec): 8.405000
Message/sec(total): 602126
  Message/sec(med): 609007
  Message/sec(max): 619776
   Latency ms(med): 1.635923
   Latency ms(max): 1.709414

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         336 186
Message/sec(med) median:      338 688
Message/sec(med) max:         341 755

== Median report:

   Messages(total): 5003124
     Duration(sec): 14.608000
Message/sec(total): 342492
  Message/sec(med): 338688
  Message/sec(max): 362109
   Latency ms(med): 2.926802
   Latency ms(max): 2.968205
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 1000 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 114 479 messages per second;
* x1.225 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         114 226
Message/sec(med) median:      114 479
Message/sec(med) max:         115 570

== Median report:

   Messages(total): 2007130
     Duration(sec): 17.710000
Message/sec(total): 113333
  Message/sec(med): 114479
  Message/sec(max): 117159
   Latency ms(med): 8.725044
   Latency ms(max): 10.152230

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          93 344
Message/sec(med) median:       93 490
Message/sec(med) max:          94 987

== Median report:

   Messages(total): 2004108
     Duration(sec): 21.312000
Message/sec(total): 94036
  Message/sec(med): 93490
  Message/sec(max): 97710
   Latency ms(med): 10.609342
   Latency ms(max): 10.950025
```
</details>

---
**10 threads 100kb messages**

```
Command line args: -thread_count 10 -connect_count_per_port 1000 -message_payload_size 102400
Run count: 5
Summary:
* 37 659 messages per second;
* x1.775 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          37 530
Message/sec(med) median:       37 659
Message/sec(med) max:          37 989

== Median report:

   Messages(total): 801224
     Duration(sec): 21.514000
Message/sec(total): 37241
  Message/sec(med): 37659
  Message/sec(max): 38166
   Latency ms(med): 26.562961
   Latency ms(max): 37.849462

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          20 724
Message/sec(med) median:       21 220
Message/sec(med) max:          21 681

== Median report:

   Messages(total): 100505
     Duration(sec): 4.802000
Message/sec(total): 20929
  Message/sec(med): 21220
  Message/sec(max): 21467
   Latency ms(med): 46.769218
   Latency ms(max): 47.220788
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 10 -connect_count_per_port 1000 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 5
Summary:
* 2 546 825 messages per second;
* x1.581 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       2 536 287
Message/sec(med) median:    2 546 825
Message/sec(med) max:       2 615 275

== Median report:

   Messages(total): 20035245
     Duration(sec): 8.005000
Message/sec(total): 2502841
  Message/sec(med): 2546825
  Message/sec(max): 2556014
   Latency ms(med): 1.964303
   Latency ms(max): 2.175952

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       1 594 898
Message/sec(med) median:    1 611 287
Message/sec(med) max:       1 664 614

== Median report:

   Messages(total): 20101105
     Duration(sec): 12.406000
Message/sec(total): 1620272
  Message/sec(med): 1611287
  Message/sec(max): 1665387
   Latency ms(med): 3.093927
   Latency ms(max): 3.170195
```
</details>

---
**Massively parallel**

```
Command line args: -thread_count 20 -connect_count_per_port 5000 -message_payload_size 128 -message_target_count 10000000
Run count: 5
Summary:
* 683 911 messages per second;
* x2.186 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         681 141
Message/sec(med) median:      683 911
Message/sec(med) max:         684 789

== Median report:

   Messages(total): 10028739
     Duration(sec): 14.909000
Message/sec(total): 672663
  Message/sec(med): 683911
  Message/sec(max): 689684
   Latency ms(med): 7.302698
   Latency ms(max): 7.657709

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         308 292
Message/sec(med) median:      312 847
Message/sec(med) max:         317 201

== Median report:

   Messages(total): 10015201
     Duration(sec): 31.919000
Message/sec(total): 313769
  Message/sec(med): 312847
  Message/sec(max): 321314
   Latency ms(med): 15.971300
   Latency ms(max): 16.211111
```
</details>
