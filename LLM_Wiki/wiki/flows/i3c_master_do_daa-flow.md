---
date: 2026-06-08
tags: [code, flow, linux-i3c]
type: concept
status: active
source-repo: linux-i3c
entry-point: i3c_master_do_daa
---

# I3C Dynamic Address Assignment Flow

## What this flow does
TBD — Entry function: `i3c_master_do_daa()` at `drivers/i3c/master.c:1860`

## Entry Point
- **Function**: `i3c_master_do_daa()`
- **File**: `drivers/i3c/master.c:1860`

## Call Chain

```
i3c_master_do_daa()  [daa]  drivers/i3c/master.c:1860
  i3c_master_do_daa_ext()  [daa]  drivers/i3c/master.c:1819
    i3c_master_rpm_get()  [core]  drivers/i3c/master.c:109
    i3c_bus_maintenance_lock()  [core]  drivers/i3c/master.c:45
    i3c_master_rstdaa_locked()  [core]  drivers/i3c/master.c:1022
      i3c_bus_get_addr_slot_status()  [core]  drivers/i3c/master.c:394
      i3c_ccc_cmd_dest_init()  [core]  drivers/i3c/master.c:899
      i3c_ccc_cmd_init()  [core]  drivers/i3c/master.c:917
      i3c_master_send_ccc_cmd_locked()  [core]  drivers/i3c/master.c:936
      i3c_ccc_cmd_dest_cleanup()  [core]  drivers/i3c/master.c:912
    i3c_bus_maintenance_unlock()  [core]  drivers/i3c/master.c:59
    i3c_bus_normaluse_lock()  [core]  drivers/i3c/master.c:80
    i3c_master_register_new_i3c_devs()  [core]  drivers/i3c/master.c:1765
    i3c_bus_normaluse_unlock()  [core]  drivers/i3c/master.c:93
    i3c_master_rpm_put()  [core]  drivers/i3c/master.c:120
```

## Key Functions in This Flow

| Function | File | Line | Layer |
|----------|------|------|-------|
| `i3c_master_do_daa_ext()` | `drivers/i3c/master.c` | 1819 | daa |

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
