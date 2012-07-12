#!/usr/bin/perl

# Create a relay test. rank n sends a message to rank n+1. When the
# message reaches the last rank, the test ends.

# Set num_ranks to the number of desired ranks. Run the test with the
# same number of ranks.

use warnings;

my $num_ranks=9;
my $rank;

print <<EOF;
<?xml version="1.0"?>
<test>
  <subtest>
    <desc>Message relay amongst $num_ranks ranks</desc>
    <ptl>
      <ptl_ni ni_opt="MATCH PHYSICAL">
	  <ompi_rt>
      <ptl_ni ni_opt="MATCH LOGICAL">
        <ptl_eq>
          <ptl_pt>
            <ptl_get_id/>
            
            <if rank="0">
              <ptl_md md_data="0xaa">
                
                <!-- Wait for rank 1 to set up the receiving buffer -->
                <barrier/>
                
                <ptl_put ack_req="ACK" length="4" match="0x5555" target_id="1"/>
                
                <!-- Wait for the packet containing the ack -->
                <ptl_eq_wait>
                  <check event_type="SEND"/>
                </ptl_eq_wait>
                
                <!-- Wait for the ack -->
                <ptl_eq_wait>
                  <check event_type="ACK"/>
                </ptl_eq_wait>
                
              </ptl_md>
            </if>
EOF


for ($rank = 1; $rank < $num_ranks-1; $rank++) {
	my $next_rank = $rank+1;
print <<EOF

            <if rank="$rank">
              <ptl_me me_opt="OP_PUT" me_match="0x5555" me_data="0x77">
              <ptl_md md_data="0xaa">
                

                <!-- Force rank 0 to wait for the buffer to be ready -->
                <barrier/>
                
                <!-- Wait for the PUT from rank 0 -->                
                <ptl_eq_wait>
                  <check event_type="PUT"/>
                </ptl_eq_wait>
                
                <check length="4" me_data="0xaa"/>



				<!-- Send to rank $next_rank -->
				<ptl_put ack_req="ACK" length="4" match="0x5555" target_id="$next_rank"/>
                
                <!-- Wait for the packet containing the ack -->
                <ptl_eq_wait>
                  <check event_type="SEND"/>
                </ptl_eq_wait>
                
                <!-- Wait for the ack -->
                <ptl_eq_wait>
                  <check event_type="ACK"/>
                </ptl_eq_wait>

			  </ptl_md>                
              </ptl_me>
            </if>
EOF
}

print <<EOF
            <if rank="$rank">
              <ptl_me me_opt="OP_PUT" me_match="0x5555" me_data="0x77">
                
                <!-- Force rank 0 to wait for the buffer to be ready -->
                <barrier/>
                
                <!-- Wait for the PUT from rank 1 -->                
                <ptl_eq_wait>
                  <check event_type="PUT"/>
                </ptl_eq_wait>
                
                <check length="4" me_data="0xaa"/>
              </ptl_me>
			</if>
            
          </ptl_pt>
        </ptl_eq>
	<barrier/>
      </ptl_ni>
	  </ompi_rt>
      </ptl_ni>
    </ptl>
    
  </subtest>
</test>
EOF
