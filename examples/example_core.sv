module example_core;
  import hdl_probe_pkg::*;

  logic [3:0] observed;
  real analog_value;

  initial begin : stimulus
    observed = 4'h0;
    analog_value = 0.5;
    #10ns observed = 4'ha;
    #10ns analog_value = 1.25;
  end

  initial begin : test
    hdl_probe probe;
    hdl_probe_value value;
    hdl_probe_status_e status;
    logic [HdlProbeMaxW-1:0] bits;
    int unsigned n_bits;
    real real_value;

    probe  = hdl_probe::get("example_core.observed");
    status = probe.read(value);
    if (status != HDL_PROBE_OK || value == null || !value.get_logic(
            bits, n_bits
        ) || n_bits != 4 || bits[3:0] !== 4'h0) begin
      $fatal(1, "initial read failed status=%0d", status);
    end

    probe.wait_for_change(value, status);
    if (status != HDL_PROBE_OK || value == null || !value.get_logic(
            bits, n_bits
        ) || bits[3:0] !== 4'ha) begin
      $fatal(1, "logic wait failed status=%0d", status);
    end

    probe = hdl_probe::get("example_core.analog_value");
    probe.wait_for_change(value, status);
    if (status != HDL_PROBE_OK || value == null || !value.get_real(
            real_value
        ) || real_value != 1.25) begin
      $fatal(1, "real wait failed status=%0d", status);
    end

    $display("example_core passed");
    #1ns;
    $finish;
  end
endmodule
