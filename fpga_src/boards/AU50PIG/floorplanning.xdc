create_pblock cmac_fifos_pblock
add_cells_to_pblock [get_pblocks cmac_fifos_pblock] [get_cells -quiet [list {core_inst/MAC_async_FIFO[0].mac_rx_async_fifo_inst} {core_inst/MAC_async_FIFO[0].mac_tx_async_fifo_inst}]]
resize_pblock [get_pblocks cmac_fifos_pblock] -add {SLICE_X8Y300:SLICE_X8Y419 SLICE_X0Y245:SLICE_X7Y476}
resize_pblock [get_pblocks cmac_fifos_pblock] -add {BUFG_GT_X0Y120:BUFG_GT_X0Y167}
resize_pblock [get_pblocks cmac_fifos_pblock] -add {BUFG_GT_SYNC_X0Y75:BUFG_GT_SYNC_X0Y104}
resize_pblock [get_pblocks cmac_fifos_pblock] -add {DSP48E2_X0Y92:DSP48E2_X0Y183}
resize_pblock [get_pblocks cmac_fifos_pblock] -add {LAGUNA_X0Y130:LAGUNA_X1Y353}
set_property IS_SOFT TRUE [get_pblocks cmac_fifos_pblock]




#PCIE
create_pblock pcie_pblock
add_cells_to_pblock [get_pblocks pcie_pblock] [get_cells -quiet [list \
          core_inst/pcie_controller_inst/cq_reg \
          core_inst/pcie_controller_inst/dma_if_pcie_us_inst \
          core_inst/pcie_controller_inst/pcie_cont_read_inst \
          core_inst/pcie_controller_inst/pcie_cont_write_inst \
          core_inst/pcie_controller_inst/pcie_us_axil_master_inst \
          core_inst/pcie_controller_inst/rc_reg \
          core_inst/pcie_controller_inst/status_error_cor_pm_inst \
          core_inst/pcie_controller_inst/status_error_uncor_pm_inst \
          core_inst/pcie_controller_inst/virtual_ports.dma_if_mux_inst \
          pcie4c_uscale_plus_inst \
          pcie_us_cfg_inst \
          pcie_us_msi_inst]]
resize_pblock [get_pblocks pcie_pblock] -add {SLICE_X183Y0:SLICE_X232Y239}
resize_pblock [get_pblocks pcie_pblock] -add {BUFG_GT_X1Y0:BUFG_GT_X1Y95}
resize_pblock [get_pblocks pcie_pblock] -add {BUFG_GT_SYNC_X1Y0:BUFG_GT_SYNC_X1Y59}
resize_pblock [get_pblocks pcie_pblock] -add {DSP48E2_X26Y0:DSP48E2_X31Y89}
resize_pblock [get_pblocks pcie_pblock] -add {LAGUNA_X26Y0:LAGUNA_X31Y119}
resize_pblock [get_pblocks pcie_pblock] -add {PCIE4CE4_X1Y0:PCIE4CE4_X1Y1}
resize_pblock [get_pblocks pcie_pblock] -add {RAMB18_X12Y0:RAMB18_X13Y95}
resize_pblock [get_pblocks pcie_pblock] -add {RAMB36_X12Y0:RAMB36_X13Y47}
resize_pblock [get_pblocks pcie_pblock] -add {URAM288_X4Y0:URAM288_X4Y63}





create_pblock Switch_n_MSGs
add_cells_to_pblock [get_pblocks Switch_n_MSGs] [get_cells -quiet {core_inst/data_in_sw/grow.axis_switch_2lvl_grow_inst/sw_lvl1 {core_inst/data_in_sw/grow.axis_switch_2lvl_grow_inst/output_registers[*].output_register/slr_source} core_inst/data_out_sw/shrink.axis_switch_2lvl_shrink_inst/last_level_sw.sw_lvl2 {core_inst/data_out_sw/shrink.axis_switch_2lvl_shrink_inst/input_registers[*].input_register/slr_dest} core_inst/ctrl_in_sw/grow.axis_switch_2lvl_grow_inst/sw_lvl1 core_inst/dram_ctrl_in_sw/grow.axis_switch_2lvl_grow_inst/sw_lvl1 core_inst/dram_ctrl_out_sw/shrink.axis_switch_2lvl_shrink_inst/last_level_arbiter.sw_lvl2 {core_inst/MAC_async_FIFO[*].mac_rx_pipeline/slr_dest} {core_inst/MAC_async_FIFO[*].mac_tx_pipeline/slr_source} core_inst/interface_incoming_stat core_inst/interface_outgoing_stat core_inst/loopback_msg_fifo_inst {core_inst/MAC_async_FIFO[0].mac_rx_fifo_inst} {core_inst/MAC_async_FIFO[1].mac_rx_fifo_inst} sync_reset_125mhz_inst}]
resize_pblock [get_pblocks Switch_n_MSGs] -add {SLICE_X0Y187:SLICE_X176Y239 SLICE_X152Y16:SLICE_X176Y186}
resize_pblock [get_pblocks Switch_n_MSGs] -add {DSP48E2_X21Y2:DSP48E2_X24Y89 DSP48E2_X0Y70:DSP48E2_X20Y89}
resize_pblock [get_pblocks Switch_n_MSGs] -add {RAMB18_X0Y76:RAMB18_X10Y95}
resize_pblock [get_pblocks Switch_n_MSGs] -add {RAMB36_X0Y38:RAMB36_X10Y47}
resize_pblock [get_pblocks Switch_n_MSGs] -add {URAM288_X3Y8:URAM288_X3Y63 URAM288_X0Y52:URAM288_X2Y63}













