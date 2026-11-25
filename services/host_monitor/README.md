# Host monitor

Optional Linux read-only service (`OPENEMBER_SERVICE_HOST_MONITOR`). It publishes `host/v1/HostResourceSnapshot` on `/host/resources` through OpenEmber Link and supports launch_manager readiness notification. No hardware-control permission is provided.

```sh
host_monitor --robot DEMO_ROBOT --connect tcp/127.0.0.1:7547 --host-id main-computer
```

`--period-ms` defaults to 1000 (range 100..10000). Host ID defaults to the hostname; use a stable explicit ID for multi-computer products and select it with Web `--host-id`. Instance IDs change on restart. Readiness means endpoints initialized, not that the first CPU differential sample is valid. SIGINT/SIGTERM close the service; subscribers also expire missing publications.

CPU, memory, uptime, load, time and OS metadata do not require root. Disk collection is opt-in with `--sample-disks`, and is restricted to local filesystem types. CPU first samples, counter resets, topology changes and missing inputs are invalid observations, not 0% use. Memory uses MemAvailable; the fallback is marked estimated. The service reports machine resources (not a container workload's cgroup limits).

See `docs/design/host-and-network-services.md` and openember-msgs `docs/host-resources-contract.md`. Resource monitoring and future network management have separate responsibilities and processes.

The isolated sampler test can run without middleware:

```sh
c++ -std=c++17 -I services/host_monitor services/host_monitor/test/sampler_test.cpp services/host_monitor/host_sampler.cpp -o /tmp/host-sampler-test
/tmp/host-sampler-test
```
