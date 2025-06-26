create_pblock pblock_1
add_cells_to_pblock [get_pblocks pblock_1] [get_cells -quiet [list \
          {core_inst/rpus[0].rpu_PR_inst} \
          {core_inst/rpus[0].rpu_intercon_inst} \
          {core_inst/rpus[1].rpu_PR_inst} \
          {core_inst/rpus[1].rpu_intercon_inst} \
          {core_inst/rpus[2].rpu_PR_inst} \
          {core_inst/rpus[2].rpu_intercon_inst} \
          {core_inst/rpus[3].rpu_PR_inst} \
          {core_inst/rpus[3].rpu_intercon_inst} \
          {core_inst/rpus[4].rpu_PR_inst} \
          {core_inst/rpus[4].rpu_intercon_inst}]]
resize_pblock [get_pblocks pblock_1] -add {SLICE_X31Y269:SLICE_X229Y471}
resize_pblock [get_pblocks pblock_1] -add {DSP48E2_X3Y102:DSP48E2_X30Y181}
resize_pblock [get_pblocks pblock_1] -add {LAGUNA_X4Y178:LAGUNA_X31Y343}
resize_pblock [get_pblocks pblock_1] -add {RAMB18_X2Y108:RAMB18_X13Y187}
resize_pblock [get_pblocks pblock_1] -add {RAMB36_X2Y54:RAMB36_X13Y93}
resize_pblock [get_pblocks pblock_1] -add {URAM288_X0Y72:URAM288_X4Y123}
create_pblock pblock_2
add_cells_to_pblock [get_pblocks pblock_2] [get_cells -quiet [list \
          {core_inst/rpus[5].rpu_PR_inst} \
          {core_inst/rpus[5].rpu_PR_inst} \
          {core_inst/rpus[6].rpu_PR_inst} \
          {core_inst/rpus[6].rpu_intercon_inst} \
          {core_inst/rpus[7].rpu_PR_inst} \
          {core_inst/rpus[7].rpu_intercon_inst}]]
resize_pblock [get_pblocks pblock_2] -add {SLICE_X0Y10:SLICE_X103Y178}
resize_pblock [get_pblocks pblock_2] -add {DSP48E2_X0Y0:DSP48E2_X13Y63}
resize_pblock [get_pblocks pblock_2] -add {RAMB18_X0Y4:RAMB18_X6Y69}
resize_pblock [get_pblocks pblock_2] -add {RAMB36_X0Y2:RAMB36_X6Y34}
resize_pblock [get_pblocks pblock_2] -add {URAM288_X0Y4:URAM288_X1Y43}
create_pblock pblock_lb_PR_inst
add_cells_to_pblock [get_pblocks pblock_lb_PR_inst] [get_cells -quiet [list core_inst/lb_PR_inst]]
resize_pblock [get_pblocks pblock_lb_PR_inst] -add {SLICE_X110Y11:SLICE_X142Y169}
resize_pblock [get_pblocks pblock_lb_PR_inst] -add {DSP48E2_X15Y0:DSP48E2_X18Y61}
resize_pblock [get_pblocks pblock_lb_PR_inst] -add {RAMB18_X7Y6:RAMB18_X8Y67}
resize_pblock [get_pblocks pblock_lb_PR_inst] -add {RAMB36_X7Y3:RAMB36_X8Y33}
resize_pblock [get_pblocks pblock_lb_PR_inst] -add {URAM288_X2Y4:URAM288_X2Y43}

