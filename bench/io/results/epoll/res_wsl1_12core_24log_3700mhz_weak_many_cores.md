**OS**: WSL 1 Linux 5.10.16.3-22631-Microsoft on Windows 11 Home 23H2<br>
**CPU**: AMD Ryzen 9 5900X 12-Core Processor 3.70 GHz, 12 Cores, 24 Logical Processors

---
**Basic**

```
Command line args: -thread_count 10 -connect_count_per_port 500 -message_payload_size 128 -message_int_count 5 -message_target_count 5000000
Run count: 5
Summary:
* 921 059 messages per second;
* x1.512 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         907 942
Message/sec(med) median:      921 059
Message/sec(med) max:         927 274

== Median report:

   Messages(total): 5038214
     Duration(sec): 5.604000
Message/sec(total): 899038
  Message/sec(med): 921059
  Message/sec(max): 928396
   Latency ms(med): 0.541244
   Latency ms(max): 0.645764

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         605 861
Message/sec(med) median:      609 030
Message/sec(med) max:         614 712

== Median report:

   Messages(total): 5011328
     Duration(sec): 8.307000
Message/sec(total): 603265
  Message/sec(med): 609030
  Message/sec(max): 613320
   Latency ms(med): 0.817909
   Latency ms(max): 0.829520
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 500 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 400 660 messages per second;
* x1.472 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         398 469
Message/sec(med) median:      400 660
Message/sec(med) max:         401 120

== Median report:

   Messages(total): 2007145
     Duration(sec): 5.103000
Message/sec(total): 393326
  Message/sec(med): 400660
  Message/sec(max): 418829
   Latency ms(med): 1.208844
   Latency ms(max): 1.611556

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         269 012
Message/sec(med) median:      272 249
Message/sec(med) max:         279 134

== Median report:

   Messages(total): 2006153
     Duration(sec): 7.405000
Message/sec(total): 270918
  Message/sec(med): 272249
  Message/sec(max): 279467
   Latency ms(med): 1.824674
   Latency ms(max): 1.897719
```
</details>

---
**10 threads 100kb messages**

```
Command line args: -thread_count 10 -connect_count_per_port 600 -message_payload_size 102400
Run count: 5
Summary:
* 24 825 messages per second;
* x1.235 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          22 330
Message/sec(med) median:       24 825
Message/sec(med) max:          27 449

== Median report:

   Messages(total): 800335
     Duration(sec): 33.240000
Message/sec(total): 24077
  Message/sec(med): 24825
  Message/sec(max): 26186
   Latency ms(med): 24.145689
   Latency ms(max): 29.710149

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          19 792
Message/sec(med) median:       20 101
Message/sec(med) max:          21 314

== Median report:

   Messages(total): 100523
     Duration(sec): 4.905000
Message/sec(total): 20493
  Message/sec(med): 20101
  Message/sec(max): 20921
   Latency ms(med): 28.839587
   Latency ms(max): 29.969334
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 10 -connect_count_per_port 500 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 5
Summary:
* 4 202 329 messages per second;
* x1.444 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       3 948 638
Message/sec(med) median:    4 202 329
Message/sec(med) max:       4 258 330

== Median report:

   Messages(total): 20401990
     Duration(sec): 5.004000
Message/sec(total): 4077136
  Message/sec(med): 4202329
  Message/sec(max): 4276682
   Latency ms(med): 0.594593
   Latency ms(max): 0.698283

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       2 801 545
Message/sec(med) median:    2 910 192
Message/sec(med) max:       2 916 322

== Median report:

   Messages(total): 20175095
     Duration(sec): 7.006000
Message/sec(total): 2879688
  Message/sec(med): 2910192
  Message/sec(max): 2937140
   Latency ms(med): 0.853249
   Latency ms(max): 0.862186
```
</details>

---
**Massively parallel**

```
Command line args: -thread_count 20 -connect_count_per_port 5000 -message_payload_size 128 -message_target_count 20000000
Run count: 5
Summary:
* 968 353 messages per second;
* x1.774 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         961 685
Message/sec(med) median:      968 353
Message/sec(med) max:       1 066 044

== Median report:

   Messages(total): 20031134
     Duration(sec): 21.117000
Message/sec(total): 948578
  Message/sec(med): 968353
  Message/sec(max): 1021764
   Latency ms(med): 5.182636
   Latency ms(max): 24.627660

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         541 812
Message/sec(med) median:      545 793
Message/sec(med) max:         552 065

== Median report:

   Messages(total): 20040159
     Duration(sec): 37.167000
Message/sec(total): 539192
  Message/sec(med): 545793
  Message/sec(max): 564159
   Latency ms(med): 9.333711
   Latency ms(max): 13.315223
```
</details>
