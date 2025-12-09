#!/usr/bin/env python3
"""Quick test of the bottle simulator"""

import sys
import os
import importlib.util

# Suppress the simulator's interactive prompts
sys.stdin = open(os.devnull, 'r')

# Import the simulator module (handling hyphen in filename)
from pathlib import Path
simulator_path = Path(__file__).parent / "bottle-simulator.py"

print("Testing bottle-simulator.py...")

try:
    # Load module from file
    spec = importlib.util.spec_from_file_location("bottle_simulator", simulator_path)
    bottle_sim = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bottle_sim)

    # Get classes
    State = bottle_sim.State
    SensorType = bottle_sim.SensorType
    MemoryBottleSimulator = bottle_sim.MemoryBottleSimulator

    print("✓ Imports successful")

    # Test simulator creation
    sim = MemoryBottleSimulator()
    print("✓ Simulator created")

    # Test initial state
    assert sim.current_state == State.IDLE, "Initial state should be IDLE"
    print("✓ Initial state is IDLE")

    # Test sensor helpers
    assert not sim.is_cap_open(), "Cap should start closed"
    print("✓ Cap state correct")

    assert not sim.is_tilted(), "Bottle should start upright"
    print("✓ Tilt state correct")

    assert not sim.can_pour(), "Should not be able to pour initially"
    print("✓ Can pour logic correct")

    # Test pot selection
    sim.pot_value = 1000
    assert sim.read_sensor_selection() == SensorType.AUDIO
    print("✓ Pot selects AUDIO for low values")

    sim.pot_value = 3000
    assert sim.read_sensor_selection() == SensorType.COLOR
    print("✓ Pot selects COLOR for high values")

    # Test edge detection
    sim.button_state = 0
    sim.previous_button_state = 0
    assert not sim.cap_just_opened()
    print("✓ Edge detection (no change)")

    sim.button_state = 1
    sim.previous_button_state = 0
    assert sim.cap_just_opened()
    print("✓ Edge detection (cap opened)")

    sim.button_state = 0
    sim.previous_button_state = 1
    assert sim.cap_just_closed()
    print("✓ Edge detection (cap closed)")

    # Test recording count
    sim.has_audio = False
    sim.has_color = False
    assert sim.get_recording_count() == 0
    print("✓ Recording count = 0")

    sim.has_audio = True
    assert sim.get_recording_count() == 1
    print("✓ Recording count = 1")

    sim.has_color = True
    assert sim.get_recording_count() == 2
    print("✓ Recording count = 2")

    print("\n" + "="*60)
    print("ALL TESTS PASSED!")
    print("="*60)
    print("\nThe simulator is ready to use. Run it with:")
    print("  python bottle-simulator.py")
    print("\nThen try commands like:")
    print("  > status")
    print("  > pot mic")
    print("  > cap open")
    print("  > cap close")

except Exception as e:
    print(f"\n✗ Test failed: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
