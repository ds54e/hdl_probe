# hdl_probe

`hdl_probe` is a DSim HDL path probe library centered on a single core package, `hdl_probe_pkg`. It supports packed 4-state logic (`0/1/X/Z`) and `real`.
The DPI declarations live in that package file, so the SV side carries as a single package source.

## Examples

Detailed read style:

```systemverilog
import hdl_probe_pkg::*;

hdl_probe probe;
hdl_probe_value value;
hdl_probe_status_e status;
logic [HdlProbeMaxW-1:0] bits;
int unsigned n_bits;

probe = hdl_probe::get("top.dut.state");
status = probe.read(value);
if (status == HDL_PROBE_OK && value.get_logic(bits, n_bits)) begin
  // use bits[n_bits-1:0]
end
```

Typed helper style:

```systemverilog
import uvm_pkg::*;
import hdl_probe_pkg::*;
`include "uvm_macros.svh"

logic [7:0] bits;
real temp;

if (!hdl_probe::read_logic("top.dut.state", bits)) begin
  `uvm_fatal("READ", "hdl_probe::read_logic failed")
end

if (!hdl_probe::read_real("top.dut.temp", temp)) begin
  `uvm_fatal("READ", "hdl_probe::read_real failed")
end

hdl_probe::wait_logic_change("top.dut.state", bits);
hdl_probe::wait_real_change("top.dut.temp", temp);
```

The typed helper APIs can write into a narrower packed variable when the caller already knows the signal width. For example, an 8-bit signal can be read into `logic [7:0] bits`.

## Notes

- The provided build and examples are DSim-oriented.
- DSim needs `+acc+rwcbf` for the VPI path lookup and value-change callback flow used here.
- `connect()` can return `HDL_PROBE_TOO_WIDE` when a packed logic object exceeds `HdlProbeMaxW`.
- When the caller needs precise failure reason or actual packed width, prefer `hdl_probe::get(path)` plus instance `read()` / `wait_for_change()` over the `bit`-returning typed helpers.
- `hdl_probe::cleanup()` is a full reset: it destroys live probes, clears the path / key caches, resets key allocation, and drops the captured callback scope.
