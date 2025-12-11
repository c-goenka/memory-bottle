import serial
import pygame
import os
import time
import threading

pygame.mixer.init()
pygame.mixer.set_num_channels(16)  # Increase available channels

SEQUENCE_CMD = 'PLAYBACK (Sequence):'
CHORD_CMD = 'CMD:CHORD:'
PLAY_DURATION_NOTE = 1  # in seconds (length of each note)
PLAY_DURATION_CHORD = 10  # in seconds (length of each chord)
FADE_DURATION = 100  # in milliseconds (fade out time)

# Reserve channels: 0-7 for sequences, 8-15 for chords
SEQUENCE_CHANNELS = list(range(0, 8))
CHORD_CHANNELS = list(range(8, 16))

sequence_recordings = []
chord_recordings = []
stop_playback = False
sequence_thread = None
chord_thread = None


def play_sound_file(note):
    filename = f"{note}.wav"
    filepath = os.path.join("Sounds", filename)
    if os.path.exists(filepath):
        return pygame.mixer.Sound(filepath)
    return None


def sleep_interruptible(duration_sec):
    for _ in range(int(duration_sec * 10)):
        if stop_playback:
            return True
        time.sleep(0.1)
    return False


def play_sequence(notes):
    global stop_playback

    print(f"  Sequential: {notes}")
    channel = pygame.mixer.Channel(SEQUENCE_CHANNELS[0])

    for note in notes:
        if stop_playback:
            return

        sound = play_sound_file(note)
        if sound:
            channel.play(sound)
            print(f"    Playing {note}")

            if sleep_interruptible(PLAY_DURATION_NOTE):
                return
            channel.stop()
            time.sleep(0.05)  # Small gap between notes


def play_chord(notes):
    global stop_playback

    print(f"  Chord (simultaneous): {notes}")
    channels = []
    for i, note in enumerate(notes):
        if i < len(CHORD_CHANNELS):
            channel = pygame.mixer.Channel(CHORD_CHANNELS[i])
            sound = play_sound_file(note)
            if sound:
                channel.play(sound)
                channels.append(channel)

    if sleep_interruptible(PLAY_DURATION_CHORD):
        return

    for channel in channels:
        channel.stop()
    time.sleep(0.05)  # Small gap between chords


def play_all_sequences():
    global stop_playback

    while not stop_playback:
        for i, notes in enumerate(sequence_recordings):
            if stop_playback:
                return
            print(f"Playing sequence {i+1}/{len(sequence_recordings)}")
            play_sequence(notes)


def play_all_chords():
    global stop_playback

    while not stop_playback:
        for i, notes in enumerate(chord_recordings):
            if stop_playback:
                return
            print(f"Playing chord {i+1}/{len(chord_recordings)}")
            play_chord(notes)


def handle_new_recording(notes, is_chord):
    global stop_playback, sequence_thread, chord_thread

    # Stop existing playback
    stop_playback = True
    if sequence_thread and sequence_thread.is_alive():
        sequence_thread.join()
    if chord_thread and chord_thread.is_alive():
        chord_thread.join()
    pygame.mixer.fadeout(FADE_DURATION)
    time.sleep(FADE_DURATION / 1000)

    # Add new recording
    if is_chord:
        chord_recordings.append(notes)
        print(f"Added chord recording. Total: {len(sequence_recordings)} sequences, {len(chord_recordings)} chords")
    else:
        sequence_recordings.append(notes)
        print(f"Added sequence recording. Total: {len(sequence_recordings)} sequences, {len(chord_recordings)} chords")

    # Restart both threads
    stop_playback = False
    if len(sequence_recordings) > 0:
        sequence_thread = threading.Thread(target=play_all_sequences)
        sequence_thread.start()
    if len(chord_recordings) > 0:
        chord_thread = threading.Thread(target=play_all_chords)
        chord_thread.start()



# Arduino setup
ser = serial.Serial('/dev/cu.usbmodem1301', 9600)  # '/dev/cu.usbmodem1101' for Mac and 'COM5' for 'COM3' for Windows
print("Listening for audio commands from Arduino...")

while True:
    if ser.in_waiting > 0:
        command = ser.readline().decode('utf-8').strip()
        print(f"\nReceived: {command}")

        if command.startswith(SEQUENCE_CMD):
            notes = command.split(":")[1].strip().split()
            handle_new_recording(notes, is_chord=False)
        elif command.startswith(CHORD_CMD):
            # Parse chord format: "CMD:CHORD:B,D,G,"
            notes_str = command.split(":", 2)[2].strip()
            notes = [n for n in notes_str.split(",") if n]  # Remove empty strings
            handle_new_recording(notes, is_chord=True)



# # For testing without Arduino:
# test_commands = [
#     "PLAYBACK (Sequence): G B D",
#     "PLAYBACK (Sequence): B D",
#     "CMD:CHORD:G,B,D,",
#     "CMD:CHORD:B,D,",
# ]

# print("Testing audio commands...")
# for command in test_commands:
#     print(f"\nReceived: {command}")
#     if command.startswith(SEQUENCE_CMD):
#         notes = command.split(":")[1].strip().split()
#         handle_new_recording(notes, is_chord=False)
#     elif command.startswith(CHORD_CMD):
#         notes_str = command.split(":", 2)[2].strip()
#         notes = [n for n in notes_str.split(",") if n]
#         handle_new_recording(notes, is_chord=True)
#     time.sleep(5)
