**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: 12th Gen Intel(R) Core(TM) i7-12800HX 0.8-4.8GHz, 24 CPUs, 2 threads per core

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2500000
Run count: 3
Summary:
* 218 499 messages per second;
* x1.149 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         217 343
Message/sec(med) median:      218 499
Message/sec(med) max:         218 641

== Median report:

   Messages(total): 2519180
     Duration(sec): 11.612000
Message/sec(total): 216946
  Message/sec(med): 218499
  Message/sec(max): 220847
   Latency ms(med): 0.458077
   Latency ms(max): 0.535153

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         189 767
Message/sec(med) median:      190 200
Message/sec(med) max:         190 299

== Median report:

   Messages(total): 2501331
     Duration(sec): 13.115000
Message/sec(total): 190722
  Message/sec(med): 190200
  Message/sec(max): 194480
   Latency ms(med): 0.523307
   Latency ms(max): 0.545478
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 245 472 messages per second;
* x1.270 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         244 975
Message/sec(med) median:      245 472
Message/sec(med) max:         248 150

== Median report:

   Messages(total): 2013152
     Duration(sec): 8.217000
Message/sec(total): 244998
  Message/sec(med): 245472
  Message/sec(max): 247642
   Latency ms(med): 0.407342
   Latency ms(max): 0.416315

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         191 704
Message/sec(med) median:      193 232
Message/sec(med) max:         194 766

== Median report:

   Messages(total): 2006244
     Duration(sec): 10.415000
Message/sec(total): 192630
  Message/sec(med): 193232
  Message/sec(max): 194258
   Latency ms(med): 0.518280
   Latency ms(max): 0.541061
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400
Run count: 3
Summary:
* 13 254 messages per second;
* x1.478 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          13 207
Message/sec(med) median:       13 254
Message/sec(med) max:          13 260

== Median report:

   Messages(total): 201283
     Duration(sec): 15.228000
Message/sec(total): 13217
  Message/sec(med): 13254
  Message/sec(max): 13515
   Latency ms(med): 15.105494
   Latency ms(max): 17.256410

#### boost::asio network
== Aggregated report:
Message/sec(med) min:           5 123
Message/sec(med) median:        8 965
Message/sec(med) max:           9 049

== Median report:

   Messages(total): 150062
     Duration(sec): 16.742000
Message/sec(total): 8963
  Message/sec(med): 8965
  Message/sec(max): 9131
   Latency ms(med): 22.266087
   Latency ms(max): 22.649819
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 1 038 372 messages per second;
* x1.129 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       1 035 317
Message/sec(med) median:    1 038 372
Message/sec(med) max:       1 046 817

== Median report:

   Messages(total): 20023130
     Duration(sec): 19.329000
Message/sec(total): 1035911
  Message/sec(med): 1038372
  Message/sec(max): 1068912
   Latency ms(med): 0.480850
   Latency ms(max): 0.555314

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         910 650
Message/sec(med) median:      919 758
Message/sec(med) max:         921 519

== Median report:

   Messages(total): 20041290
     Duration(sec): 21.822000
Message/sec(total): 918398
  Message/sec(med): 919758
  Message/sec(max): 937820
   Latency ms(med): 0.544225
   Latency ms(max): 0.569587
```
</details>
