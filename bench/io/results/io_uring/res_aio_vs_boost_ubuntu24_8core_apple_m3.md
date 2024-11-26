**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: Apple M3 8 cores

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2500000
Run count: 3
Summary:
* 286 258 messages per second;
* x1.217 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         284 487
Message/sec(med) median:      286 258
Message/sec(med) max:         288 425

== Median report:

   Messages(total): 2501979
     Duration(sec): 8.886000
Message/sec(total): 281564
  Message/sec(med): 286258
  Message/sec(max): 288041
   Latency ms(med): 0.352417
   Latency ms(max): 0.407030

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         227 696
Message/sec(med) median:      235 121
Message/sec(med) max:         237 637

== Median report:

   Messages(total): 2506378
     Duration(sec): 10.703000
Message/sec(total): 234175
  Message/sec(med): 235121
  Message/sec(max): 241217
   Latency ms(med): 0.423274
   Latency ms(max): 0.445038
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 223 576 messages per second;
* x1.158 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         222 203
Message/sec(med) median:      223 576
Message/sec(med) max:         224 168

== Median report:

   Messages(total): 2002391
     Duration(sec): 9.026000
Message/sec(total): 221846
  Message/sec(med): 223576
  Message/sec(max): 232835
   Latency ms(med): 0.450231
   Latency ms(max): 0.460839

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         193 017
Message/sec(med) median:      193 153
Message/sec(med) max:         193 653

== Median report:

   Messages(total): 2013656
     Duration(sec): 10.545000
Message/sec(total): 190958
  Message/sec(med): 193153
  Message/sec(max): 197790
   Latency ms(med): 0.522524
   Latency ms(max): 0.531822
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400
Run count: 3
Summary:
* 15 112 messages per second;
* x1.636 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          15 057
Message/sec(med) median:       15 112
Message/sec(med) max:          15 242

== Median report:

   Messages(total): 200423
     Duration(sec): 13.220000
Message/sec(total): 15160
  Message/sec(med): 15112
  Message/sec(max): 16102
   Latency ms(med): 13.229015
   Latency ms(max): 13.776244

#### boost::asio network
== Aggregated report:
Message/sec(med) min:           5 972
Message/sec(med) median:        9 238
Message/sec(med) max:           9 545

== Median report:

   Messages(total): 150707
     Duration(sec): 16.552000
Message/sec(total): 9105
  Message/sec(med): 9238
  Message/sec(max): 9389
   Latency ms(med): 21.800951
   Latency ms(max): 22.843602
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 1 284 047 messages per second;
* x1.181 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       1 256 022
Message/sec(med) median:    1 284 047
Message/sec(med) max:       1 291 272

== Median report:

   Messages(total): 20089055
     Duration(sec): 15.762000
Message/sec(total): 1274524
  Message/sec(med): 1284047
  Message/sec(max): 1313024
   Latency ms(med): 0.391402
   Latency ms(max): 0.409548

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       1 086 721
Message/sec(med) median:    1 087 212
Message/sec(med) max:       1 091 754

== Median report:

   Messages(total): 20115265
     Duration(sec): 18.685000
Message/sec(total): 1076546
  Message/sec(med): 1087212
  Message/sec(max): 1097234
   Latency ms(med): 0.463682
   Latency ms(max): 0.472652
```
</details>
