
### Sequence editing features ###

* TODO: Timing based events - schedule time events every Nth of a second to allow:
   - time based key presses to work
   - button blinking (this could be done with page swapping on launchpad)
   - also distribute sequence relative playback time across the code
     (or better yet - let the sequence know it is being played since ticks X)
* TODO: Distribute playback events through the code
   - sequence played note, etc. Could render those notes with different color
* TODO: Pressing shift after holding note should skip erase operation on that note after grid release
* TODO: Scroll immediately when selection goes off-screen
* TODO: Mark the boundaries of the sequence red on the arrows (can't scroll left, past sequence end, note 0/127)
* TODO: Prolonging notes while selection is active does not work
* TODO: shift long press should blink and deselects even before release
* TODO: allow scrolling when selection is visible - rework the key combinations
* TODO: shift should light up when selections are visible
* TODO: When selection is visible allow viewing/modifying the velocity values
* TODO: Allow moving the endpoint marker of the sequence - sequence length
* TODO: Copy/paste of selection with pasted notes being selected (original unselected),
  relative to view position
* TODO: Whole sequence selection? For transposition etc.
* TODO: Display playback line when playback passes over the current view - and global even propagation in general
* TODO: Alternative display/edit mode - display non-note events (modwheel, pitchbend, midi CCs)
* Triplet view could leave every fourth column empty, easing orientation in timing.

### Track screen ###

* Project will have 3(4) basic screens and some sub-screens
* Default screen should be track view screen
  - Each row is one track
  - Each track contains sequences, numbered 0-N
  - Play button press will mute/unmute the track
  - Shift play button will bring track settings (midi channel, timing, scale selection etc)

  - While playing the song (shift right arrow?) pressing tracks sequence buttons will queue the sequences to play in defined time frames. TODO: When to decide to start playing sequences in tracks?
  - Should allow copying (press one, press free space to copy there)
  - Deleting the sequences - some harder combo maybe (TODO: what about undo here?)
  - Each track should have a selectable color (not brightness! that will be used in song screen to distinguish different tracks)
  - Allow recording of sequences via MIDI input device
  - Allow changing sequence/track key (either re-mapping notes, or disallowing when the key does not fit the notes)
  - Could also do a key discovery - map sequence to a 12bit integer - compare to key bitmaps to show which fit (this could be done in the track settings screen)

### Song screen ###

* Second screen is a planned song screen, allowing scheduling sequences in time - doing arrangements
* Should display the other track's sequence placements in lower brightness for orientation
* Time goes left->right
* Sequence number top to bottom (transposed as opposed to tracks screen?!)
* TODO: Could be made top to bottom to align with track screen


### Playback/recording engine ###

* Allow scheduling sequences to given relative time ticks
* Allow more than one sequence to be played into one midi channel
* Allow passthrough for MIDI keyboard
* Allow recording sequences directly from midi keyboard

### General ###
* Add config file handling
* Add load/save support (project serialization/deserialization and UI)
* Add device discovery/hotplugging
* Allow LP entanglement - virtual multi-device LP (2x1, 1x2, 2x2...)

### Refactorings ###
* UpdateBlock could have a small base class that would hold all the common code (mark dirty...)
* Launchpad class could use a clearer abstraction - meaning we could add other matrix based controllers (newer launchpads)
