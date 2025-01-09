# Philosophy

YunetaS integrates programming concepts with a life-inspired philosophy, using G objects and events to model and handle behavior in a finite state machine (automaton). Key ideas include:

## . Events and Actions:
- Each event triggers a corresponding action or consequence.
- Events can be internal (self-generated) or external (environment-driven).
- Internal events, when positive, lead to progress; external events often highlight our lack of control.
- No events mean nothing happens—this is the “emptiness.”

## . Unity of Systems:
- “What is above is below”—by extension, “what is inside is outside.”
- Events and their actions occur over time, and understanding them requires recognizing how internal and external influences intertwine.

## . Hierarchy and Structure:
- G objects are organized in parent → child hierarchies, reflecting how complex systems and life can be structured.
- Objects can publish events to which other objects subscribe, if authorized.

## . Time and Data Collection:
- Yuneta focuses on time and event as core dimensions for data collection.
- Persistent data is stored as time-series (sequential data) and key-value pairs.
- Repeated events or actions over time lead to success or failure, emphasizing the importance of consistency.

## . Timeranger and Treedb:
- Timeranger stores time-series and key-value data both in files and in memory.
- Treedb offers a higher-level model storing parent-child links in memory, while on disk, child IDs reference parent IDs (the parent does not hold child references on disk).
- This approach creates a graph-like structure in memory upon loading the data, reflecting the interconnected nature of real-world entities.

> Overall, YunetaS philosophy encourages a holistic view of software design and life: understanding the power of events and his actions, structuring objects hierarchically, and emphasizing consistent actions and data tracking over time.
