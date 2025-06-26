/*

Copyright (c) 2019-2021 Moein Khazraee

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`resetall
`timescale 1ns / 1ps
`default_nettype none

module pcie_config # (
  parameter PCIE_ADDR_WIDTH         = 64,
  parameter PCIE_RAM_ADDR_WIDTH     = 32,
  parameter PCIE_DMA_LEN_WIDTH      = 16,
  parameter HOST_DMA_TAG_WIDTH      = 32,
  parameter AXIL_DATA_WIDTH         = 32,
  parameter AXIL_STRB_WIDTH         = (AXIL_DATA_WIDTH/8),
  parameter AXIL_ADDR_WIDTH         = 32,
  parameter IF_COUNT                = 2,
  parameter PORTS_PER_IF            = 1,
  parameter FW_ID                   = 32'd0,
  parameter FW_VER                  = {16'd0, 16'd1},
  parameter BOARD_ID                = {16'h1ce4, 16'h0003},
  parameter BOARD_VER               = {16'd0, 16'd1},
  parameter FPGA_ID                 = 32'h3823093
) (
  input  wire                           clk,
  input  wire                           rst,

  // AXI lite
  input  wire [AXIL_ADDR_WIDTH-1:0]     axil_ctrl_awaddr,
  input  wire [2:0]                     axil_ctrl_awprot,
  input  wire                           axil_ctrl_awvalid,
  output reg                            axil_ctrl_awready,
  input  wire [AXIL_DATA_WIDTH-1:0]     axil_ctrl_wdata,
  input  wire [AXIL_STRB_WIDTH-1:0]     axil_ctrl_wstrb,
  input  wire                           axil_ctrl_wvalid,
  output reg                            axil_ctrl_wready,
  output wire [1:0]                     axil_ctrl_bresp,
  output reg                            axil_ctrl_bvalid,
  input  wire                           axil_ctrl_bready,
  input  wire [AXIL_ADDR_WIDTH-1:0]     axil_ctrl_araddr,
  input  wire [2:0]                     axil_ctrl_arprot,
  input  wire                           axil_ctrl_arvalid,
  output reg                            axil_ctrl_arready,
  output reg  [AXIL_DATA_WIDTH-1:0]     axil_ctrl_rdata,
  output wire [1:0]                     axil_ctrl_rresp,
  output reg                            axil_ctrl_rvalid,
  input  wire                           axil_ctrl_rready,

  // DMA requests from Host
  output reg  [PCIE_ADDR_WIDTH-1:0]     host_dma_read_desc_pcie_addr,
  output reg  [PCIE_RAM_ADDR_WIDTH-1:0] host_dma_read_desc_ram_addr,
  output reg  [PCIE_DMA_LEN_WIDTH-1:0]  host_dma_read_desc_len,
  output reg  [HOST_DMA_TAG_WIDTH-1:0]  host_dma_read_desc_tag,
  output reg                            host_dma_read_desc_valid,
  input  wire                           host_dma_read_desc_ready,
  input  wire [HOST_DMA_TAG_WIDTH-1:0]  host_dma_read_desc_status_tag,
  input  wire                           host_dma_read_desc_status_valid,

  output reg  [PCIE_ADDR_WIDTH-1:0]     host_dma_write_desc_pcie_addr,
  output reg  [PCIE_RAM_ADDR_WIDTH-1:0] host_dma_write_desc_ram_addr,
  output reg  [PCIE_DMA_LEN_WIDTH-1:0]  host_dma_write_desc_len,
  output reg  [HOST_DMA_TAG_WIDTH-1:0]  host_dma_write_desc_tag,
  output reg                            host_dma_write_desc_valid,
  input  wire                           host_dma_write_desc_ready,
  input  wire [HOST_DMA_TAG_WIDTH-1:0]  host_dma_write_desc_status_tag,
  input  wire                           host_dma_write_desc_status_valid,

  // Host commands
  output reg  [31:0]                    host_cmd,
  output reg  [31:0]                    host_cmd_wr_data,
  input  wire [31:0]                    host_cmd_rd_data,
  output reg                            host_cmd_valid,
  input  wire                           host_cmd_ready,

  // PCIe DMA enable and interrupts
  output reg                            pcie_dma_enable,
  output reg                            corundum_loopback,
  input  wire [31:0]                    if_msi_irq,
  output wire [31:0]                    msi_irq
);

// Interface and port count, and address space allocation. If corundum is used.
parameter IF_AXIL_ADDR_WIDTH  = AXIL_ADDR_WIDTH-$clog2(IF_COUNT);
parameter AXIL_CSR_ADDR_WIDTH = IF_AXIL_ADDR_WIDTH-5-$clog2((PORTS_PER_IF+3)/8);

// State registers for readback
reg [HOST_DMA_TAG_WIDTH-1:0]  host_dma_read_status_tags;
reg [HOST_DMA_TAG_WIDTH-1:0]  host_dma_write_status_tags;

assign axil_ctrl_bresp  = 2'b00;
assign axil_ctrl_rresp  = 2'b00;



always @(posedge clk) begin
    if (rst) begin
        axil_ctrl_awready          <= 1'b0;
        axil_ctrl_wready           <= 1'b0;
        axil_ctrl_bvalid           <= 1'b0;
        axil_ctrl_arready          <= 1'b0;
        axil_ctrl_rvalid           <= 1'b0;

        host_dma_read_desc_valid   <= 1'b0;
        host_dma_write_desc_valid  <= 1'b0;
        host_cmd_valid             <= 1'b0;
        host_dma_read_status_tags  <= {HOST_DMA_TAG_WIDTH{1'b0}};
        host_dma_write_status_tags <= {HOST_DMA_TAG_WIDTH{1'b0}};
        pcie_dma_enable            <= 1'b1;
        corundum_loopback          <= 1'b0;



    end else begin

        axil_ctrl_awready <= 1'b0;
        axil_ctrl_wready  <= 1'b0;
        axil_ctrl_bvalid  <= axil_ctrl_bvalid && !axil_ctrl_bready;
        axil_ctrl_arready <= 1'b0;
        axil_ctrl_rvalid  <= axil_ctrl_rvalid && !axil_ctrl_rready;

        host_dma_read_desc_valid  <= host_dma_read_desc_valid && !host_dma_read_desc_ready;
        host_dma_write_desc_valid <= host_dma_write_desc_valid && !host_dma_write_desc_ready;
        host_cmd_valid            <= host_cmd_valid && !host_cmd_ready;

        if (axil_ctrl_awvalid && axil_ctrl_wvalid && !axil_ctrl_bvalid) begin
            // write operation
            axil_ctrl_awready <= 1'b1;
            axil_ctrl_wready  <= 1'b1;
            axil_ctrl_bvalid  <= 1'b1;

            case ({axil_ctrl_awaddr[15:2], 2'b00})
                // GPIO
                16'h0110: begin
                    // GPIO I2C 0
             
                end
                16'h0120: begin
                    // GPIO XCVR 0123
          
                end

                // Cores control
                16'h0400: pcie_dma_enable  <= axil_ctrl_wdata[0];
                16'h0404: host_cmd_wr_data <= axil_ctrl_wdata;
                16'h0408: begin
                    host_cmd       <= axil_ctrl_wdata;
                    host_cmd_valid <= 1'b1;
                end

                // DMA request
                16'h0440: host_dma_read_desc_pcie_addr[31:0] <= axil_ctrl_wdata;
                16'h0444: host_dma_read_desc_pcie_addr[63:32] <= axil_ctrl_wdata;
                16'h0448: host_dma_read_desc_ram_addr[31:0] <= axil_ctrl_wdata;
                16'h0450: host_dma_read_desc_len <= axil_ctrl_wdata;
                16'h0454: begin
                    host_dma_read_desc_tag <= axil_ctrl_wdata;
                    host_dma_read_desc_valid <= 1'b1;
                end
                16'h0460: host_dma_write_desc_pcie_addr[31:0] <= axil_ctrl_wdata;
                16'h0464: host_dma_write_desc_pcie_addr[63:32] <= axil_ctrl_wdata;
                16'h0468: host_dma_write_desc_ram_addr[31:0] <= axil_ctrl_wdata;
                16'h0470: host_dma_write_desc_len <= axil_ctrl_wdata;
                16'h0474: begin
                    host_dma_write_desc_tag <= axil_ctrl_wdata;
                    host_dma_write_desc_valid <= 1'b1;
                end
                16'h0480: corundum_loopback <= axil_ctrl_wdata[0];
            endcase
        end

        if (axil_ctrl_arvalid && !axil_ctrl_rvalid) begin
            // read operation
            axil_ctrl_arready <= 1'b1;
            axil_ctrl_rvalid  <= 1'b1;
            axil_ctrl_rdata   <= {AXIL_DATA_WIDTH{1'b0}};

            case ({axil_ctrl_araddr[15:2], 2'b00})
                16'h0000: axil_ctrl_rdata <= FW_ID;      // fw_id
                16'h0004: axil_ctrl_rdata <= FW_VER;     // fw_ver
                16'h0008: axil_ctrl_rdata <= BOARD_ID;   // board_id
                16'h000C: axil_ctrl_rdata <= BOARD_VER;  // board_ver
                16'h0010: axil_ctrl_rdata <= 0;          // phc_count
                16'h0014: axil_ctrl_rdata <= 16'h0200;   // phc_offset
                16'h0018: axil_ctrl_rdata <= 16'h0080;   // phc_stride
                16'h0020: axil_ctrl_rdata <= IF_COUNT;   // if_count
                16'h0024: axil_ctrl_rdata <= 2**IF_AXIL_ADDR_WIDTH; // if_stride
                16'h002C: axil_ctrl_rdata <= 2**AXIL_CSR_ADDR_WIDTH; // if_ctrl_offset
                16'h0040: axil_ctrl_rdata <= FPGA_ID;    // fpga_id

                // GPIO
                16'h0110: begin
                    // GPIO I2C 0
                    
                end
                16'h0120: begin
                   
                end

                // Cores control and DMA request response
                16'h0400: axil_ctrl_rdata <= pcie_dma_enable;
                16'h0404: axil_ctrl_rdata <= host_cmd_rd_data;
                16'h0458: axil_ctrl_rdata <= host_dma_read_status_tags;
                16'h0478: axil_ctrl_rdata <= host_dma_write_status_tags;
                16'h0480: axil_ctrl_rdata <= corundum_loopback;
            endcase
        end

        if (axil_ctrl_arvalid && !axil_ctrl_rvalid && ({axil_ctrl_araddr[15:2], 2'd0}==16'h0458))
            if (!host_dma_read_desc_status_valid)
                host_dma_read_status_tags <= {HOST_DMA_TAG_WIDTH{1'b0}};
            else
                host_dma_read_status_tags <= host_dma_read_desc_status_tag;
        else if (host_dma_read_desc_status_valid)
          host_dma_read_status_tags <= host_dma_read_status_tags | host_dma_read_desc_status_tag;

        if (axil_ctrl_arvalid && !axil_ctrl_rvalid && ({axil_ctrl_araddr[15:2], 2'd0}==16'h0478))
            if (!host_dma_write_desc_status_valid)
                host_dma_write_status_tags <= {HOST_DMA_TAG_WIDTH{1'b0}};
            else
                host_dma_write_status_tags <= host_dma_write_desc_status_tag;
        else if (host_dma_write_desc_status_valid)
          host_dma_write_status_tags <= host_dma_write_status_tags | host_dma_write_desc_status_tag;

    end
end

// One level register before i2c and flash input/outputs for better timing
//always @(posedge clk)
//    if (rst) begin
       
   // end else begin
      
   // end

// assign msi_irq[0] = host_dma_read_desc_status_valid || host_dma_write_desc_status_valid;
assign msi_irq = if_msi_irq;

endmodule

`resetall
