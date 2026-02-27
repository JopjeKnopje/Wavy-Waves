import pytest
from pytest_embedded import Dut

@pytest.mark.supported_targets
@pytest.mark.generic
def test_device_id(dut: Dut) -> None:
    dut.run_all_single_board_cases(name='device_id')
   
    # Wait for the output with a timeout
    dut.expect('Device ID:')
  
