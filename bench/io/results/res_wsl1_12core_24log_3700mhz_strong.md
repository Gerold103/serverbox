**OS**: WSL 1 Linux 5.10.16.3-22631-Microsoft on Windows 11 Home 23H2<br>
**CPU**: AMD Ryzen 9 5900X 12-Core Processor 3.70 GHz, 12 Cores, 24 Logical Processors

---
**Basic**

```
Command line args: -thread_count 5 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 5000000
Run count: 5
Summary:
* 706 873 messages per second;
* x1.324 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         695 457
Message/sec(med) median:      706 873
Message/sec(med) max:         710 776

== Median report:

   Messages(total): 5007435
     Duration(sec): 7.105000
Message/sec(total): 704776
  Message/sec(med): 706873
  Message/sec(max): 712742
   Latency ms(med): 0.141284
   Latency ms(max): 0.152482

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         533 028
Message/sec(med) median:      533 740
Message/sec(med) max:         535 952

== Median report:

   Messages(total): 5012275
     Duration(sec): 9.407000
Message/sec(total): 532823
  Message/sec(med): 533740
  Message/sec(max): 535275
   Latency ms(med): 0.187273
   Latency ms(max): 0.187896
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 5
Summary:
* 427 610 messages per second;
* x1.466 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         424 429
Message/sec(med) median:      427 610
Message/sec(med) max:         432 370

== Median report:

   Messages(total): 2010730
     Duration(sec): 4.803000
Message/sec(total): 418640
  Message/sec(med): 427610
  Message/sec(max): 431259
   Latency ms(med): 0.233307
   Latency ms(max): 0.294190

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         286 992
Message/sec(med) median:      291 722
Message/sec(med) max:         292 975

== Median report:

   Messages(total): 2012315
     Duration(sec): 6.905000
Message/sec(total): 291428
  Message/sec(med): 291722
  Message/sec(max): 293924
   Latency ms(med): 0.340760
   Latency ms(max): 0.343884
```
</details>

---
**5 threads 100kb messages**

```
Command line args: -thread_count 5 -connect_count_per_port 200 -message_payload_size 102400
Run count: 5
Summary:
* 37 841 messages per second;
* x1.218 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          35 452
Message/sec(med) median:       37 841
Message/sec(med) max:          38 337

== Median report:

   Messages(total): 802797
     Duration(sec): 21.420000
Message/sec(total): 37478
  Message/sec(med): 37841
  Message/sec(max): 38190
   Latency ms(med): 5.290028
   Latency ms(max): 10.307692

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          30 586
Message/sec(med) median:       31 061
Message/sec(med) max:          32 076

== Median report:

   Messages(total): 500861
     Duration(sec): 16.115000
Message/sec(total): 31080
  Message/sec(med): 31061
  Message/sec(max): 32260
   Latency ms(med): 6.407472
   Latency ms(max): 6.677111
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 5 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 5
Summary:
* 2 893 124 messages per second;
* x1.259 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       2 815 825
Message/sec(med) median:    2 893 124
Message/sec(med) max:       3 111 272

== Median report:

   Messages(total): 20066550
     Duration(sec): 7.006000
Message/sec(total): 2864194
  Message/sec(med): 2893124
  Message/sec(max): 2933941
   Latency ms(med): 0.172920
   Latency ms(max): 0.177789

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       2 247 995
Message/sec(med) median:    2 298 417
Message/sec(med) max:       2 352 277

== Median report:

   Messages(total): 20119260
     Duration(sec): 8.707000
Message/sec(total): 2310699
  Message/sec(med): 2298417
  Message/sec(max): 2341130
   Latency ms(med): 0.214996
   Latency ms(max): 0.219003
```
</details>
