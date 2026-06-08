---
date: 2026-06-08
tags: [code, flow, linux-i3c]
type: concept
status: active
source-repo: linux-i3c
entry-point: i3c_master_register
---

# I3C Master Probe Flow

## What this flow does
TBD — Entry function: `i3c_master_register()` at `drivers/i3c/master.c:3006`

## Entry Point
- **Function**: `i3c_master_register()`
- **File**: `drivers/i3c/master.c:3006`

## Call Chain

```
i3c_master_register()  [entry]  drivers/i3c/master.c:3006
  i3c_master_check_ops()  [core]  drivers/i3c/master.c:2971
  i3c_master_rpm_get()  [core]  drivers/i3c/master.c:109
  i3c_bus_init()  [core]  drivers/i3c/master.c:500
    i3c_bus_init_addrslots()  [core]  drivers/i3c/master.c:474
      i3c_bus_set_addr_slot_status()  [core]  drivers/i3c/master.c:414
  of_populate_i3c_bus()  [core]  drivers/i3c/master.c:2518
    of_i3c_master_add_dev()  [core]  drivers/i3c/master.c:2493
      of_i3c_master_add_i2c_boardinfo()  [core]  drivers/i3c/master.c:2410
      of_i3c_master_add_i3c_boardinfo()  [core]  drivers/i3c/master.c:2445
  i3c_bus_set_mode()  [core]  drivers/i3c/master.c:821
    i3c_bus_to_i3c_master()  [core]  drivers/i3c/master.c:98
  i3c_master_bus_init()  [core]  drivers/i3c/master.c:2057
    i3c_bus_get_addr_slot_status()  [core]  drivers/i3c/master.c:394
      i3c_bus_get_addr_slot_status_mask()  [core]  drivers/i3c/master.c:379
    i3c_bus_set_addr_slot_status()  [core]  drivers/i3c/master.c:414
      i3c_bus_set_addr_slot_status_mask()  [core]  drivers/i3c/master.c:400
    i3c_master_alloc_i2c_dev()  [core]  drivers/i3c/master.c:882
    i3c_master_attach_i2c_dev()  [core]  drivers/i3c/master.c:1693
    i3c_master_free_i2c_dev()  [core]  drivers/i3c/master.c:877
    i3c_master_rstdaa_locked()  [core]  drivers/i3c/master.c:1022
      i3c_bus_get_addr_slot_status()  [core]  drivers/i3c/master.c:394
      i3c_ccc_cmd_dest_init()  [core]  drivers/i3c/master.c:899
      i3c_ccc_cmd_init()  [core]  drivers/i3c/master.c:917
      i3c_master_send_ccc_cmd_locked()  [core]  drivers/i3c/master.c:936
      i3c_ccc_cmd_dest_cleanup()  [core]  drivers/i3c/master.c:912
    i3c_master_enec_disec_locked()  [core]  drivers/i3c/master.c:1085
      i3c_ccc_cmd_dest_init()  [core]  drivers/i3c/master.c:899
      i3c_ccc_cmd_init()  [core]  drivers/i3c/master.c:917
      i3c_master_send_ccc_cmd_locked()  [core]  drivers/i3c/master.c:936
      i3c_ccc_cmd_dest_cleanup()  [core]  drivers/i3c/master.c:912
    i3c_bus_set_addr_slot_status_mask()  [core]  drivers/i3c/master.c:400
    i3c_master_early_i3c_dev_add()  [core]  drivers/i3c/master.c:1719
      i3c_master_alloc_i3c_dev()  [core]  drivers/i3c/master.c:1005
      i3c_master_attach_i3c_dev()  [core]  drivers/i3c/master.c:1625
      i3c_master_setdasa_locked()  [core]  drivers/i3c/master.c:1269
      i3c_master_reattach_i3c_dev()  [core]  drivers/i3c/master.c:1655
      i3c_master_retrieve_dev_info()  [core]  drivers/i3c/master.c:1508
      i3c_master_rstdaa_locked()  [core]  drivers/i3c/master.c:1022
      i3c_master_detach_i3c_dev()  [core]  drivers/i3c/master.c:1681
      i3c_master_free_i3c_dev()  [core]  drivers/i3c/master.c:1000
    i3c_master_do_daa()  [daa]  drivers/i3c/master.c:1860
      i3c_master_do_daa_ext()  [daa]  drivers/i3c/master.c:1819
    i3c_master_detach_free_devs()  [core]  drivers/i3c/master.c:1998
      i3c_master_detach_i3c_dev()  [core]  drivers/i3c/master.c:1681
      i3c_bus_set_addr_slot_status()  [core]  drivers/i3c/master.c:414
      i3c_master_free_i3c_dev()  [core]  drivers/i3c/master.c:1000
      i3c_master_detach_i2c_dev()  [core]  drivers/i3c/master.c:1709
      i3c_master_free_i2c_dev()  [core]  drivers/i3c/master.c:877
  i3c_master_i2c_adapter_init()  [core]  drivers/i3c/master.c:2711
    i3c_master_to_i2c_adapter()  [core]  drivers/i3c/master.c:871
    i3c_master_find_i2c_dev_by_addr()  [core]  drivers/i3c/master.c:959
  i3c_bus_notify()  [core]  drivers/i3c/master.c:558
  i3c_bus_normaluse_lock()  [core]  drivers/i3c/master.c:80
  i3c_master_register_new_i3c_devs()  [core]  drivers/i3c/master.c:1765
  i3c_bus_normaluse_unlock()  [core]  drivers/i3c/master.c:93
  i3c_master_rpm_put()  [core]  drivers/i3c/master.c:120
  i3c_master_bus_cleanup()  [core]  drivers/i3c/master.c:2205
    i3c_master_rpm_get()  [core]  drivers/i3c/master.c:109
    i3c_master_rpm_put()  [core]  drivers/i3c/master.c:120
    i3c_master_detach_free_devs()  [core]  drivers/i3c/master.c:1998
```

## Key Functions in This Flow

| Function | File | Line | Layer |
|----------|------|------|-------|
| `i3c_master_check_ops()` | `drivers/i3c/master.c` | 2971 | core |
| `i3c_master_rpm_get()` | `drivers/i3c/master.c` | 109 | core |
| `i3c_bus_init()` | `drivers/i3c/master.c` | 500 | core |
| `of_populate_i3c_bus()` | `drivers/i3c/master.c` | 2518 | core |
| `i3c_bus_set_mode()` | `drivers/i3c/master.c` | 821 | core |
| `i3c_master_bus_init()` | `drivers/i3c/master.c` | 2057 | core |
| `i3c_master_i2c_adapter_init()` | `drivers/i3c/master.c` | 2711 | core |
| `i3c_bus_notify()` | `drivers/i3c/master.c` | 558 | core |
| `i3c_bus_normaluse_lock()` | `drivers/i3c/master.c` | 80 | core |
| `i3c_master_register_new_i3c_devs()` | `drivers/i3c/master.c` | 1765 | core |
| `i3c_bus_normaluse_unlock()` | `drivers/i3c/master.c` | 93 | core |
| `i3c_master_rpm_put()` | `drivers/i3c/master.c` | 120 | core |
| `i3c_master_bus_cleanup()` | `drivers/i3c/master.c` | 2205 | core |

## Register Access
(To be populated from code analysis)

## Related Concepts
- [[linux-i3c]] — linux-i3c codebase overview

## Debug Checklist
- [ ] Verify entry point conditions
- [ ] Check error return paths
- [ ] Validate register configurations

## Counter-Arguments and Gaps

...
