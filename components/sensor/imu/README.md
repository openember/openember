# IMU host installation mapping

Raw drivers retain their acquisition frames and original metadata. `ImuInstallation` / `ApplyImuInstallation` provide an explicit host-side mapping for a qualified sensor-to-reference, Z-up profile. This operation does not configure or calibrate the device, establish an absolute heading, or qualify dynamic response.

`sensor_to_body` stores a unit quaternion in wxyz order: `v_body = R(q_body_sensor) v_sensor`. The output attitude is `q_reference_body = q_reference_sensor * inverse(q_body_sensor)`. Acceleration, angular velocity and valid magnetic field vectors are all rotated consistently. Sensor sequence, acquisition domain and original receive/sample timestamps are preserved. Missing vectors remain invalid, missing orientation remains invalid, and malformed quaternions are rejected rather than repaired. Admissible small quaternion quantization error is normalized before composition.

Installation mapping defaults to disabled. HI `device.yaml` supports the following optional declaration for the qualified YESENSE profile; both `orientation` and `mounting` are required together. These declarations are trusted deployment configuration, not automatic proof that a physical installation was tested. The installation revision identifies the reviewed parameter set.

```yaml
devices:
  imu0:
    type: imu
    driver: yesense_readonly
    model: YIS321
    mode: readonly
    frame_id: imu_link
    orientation:
      convention: sensor_to_reference_z_up
      reference_frame_id: imu_local_world
    mounting:
      body_frame_id: base_link
      rotation_sensor_to_body_wxyz: [1, 0, 0, 0]
      revision: demo-installation-v1
    connection:
      device: /dev/imu_sensor
      baud_rate: 921600
      rx_poll_wait_ms: 5
```

HI applies the transform before publishing the sample. `frame_id` in the device configuration identifies the raw driver frame; the published installed sample uses `mounting.body_frame_id`. Omitted installation retains raw behavior. Raw driver registry/CLI acquisition deliberately remains raw, even when its parsed `ImuDriverConfig` contains an installation; owners that require body samples explicitly apply the mapping. No configuration option changes quaternion byte ordering, silently swaps axes, or restamps a measurement. Publication/installation validity does not itself grant motion authorization.
