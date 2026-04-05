# **Philosophy**

Yuneta integrates programming principles with a life-inspired philosophy, emphasizing the interplay of events, actions, and structures in both software and life. The framework is built around G objects, events, and finite state machines (FSMs), promoting a holistic approach to understanding systems and their interactions.

---

## **Core Principles**

### **1. Events and Actions**
- Events are the fundamental drivers of change; without events, “nothing happens”—this is the concept of **emptiness**.
- Each event triggers a corresponding action or consequence.
- Events can be:
    - **Internal**: Self-generated, representing control and the potential for progress.
    - **External**: Environment-driven, often highlighting our lack of control.
- The relationship between events and actions emphasizes cause and effect, mirroring real-world dynamics.

---

### **2. Unity of Systems**
- **“What is above is below; what is inside is outside.”**  
  This principle highlights the interconnectedness of systems, where internal dynamics reflect external behaviors and vice versa.
- Events and their consequences unfold over time, intertwining internal and external influences.

---

### **3. Hierarchy and Structure**
- G objects are organized in a **parent → child hierarchy**, mirroring the complexity of life and systems.
- Components are designed to publish and subscribe to events within an **authorized scope**, promoting structured and secure interactions.

---

### **4. Time and Data**
- Time and events are core dimensions for data collection and understanding.
- Persistent data is organized as:
    - **Time-series data**: Sequential data linked to specific events over time.
    - **Key-value pairs**: Structured data capturing relationships and attributes.
- Consistency and repetition of events or actions are key to achieving success or identifying failure.

---

## **Technological Manifestations**

### **1. Timeranger**
- A system for managing **time-series data** and key-value pairs stored in both memory and flat files.
- Provides an efficient mechanism for tracking event-driven changes over time.

### **2. Treedb**
- A graph-based database model that extends Timeranger by maintaining **parent-child relationships** in memory.
- On disk:
    - Child IDs reference their parent IDs.
    - Parent nodes do not store references to children, maintaining a lightweight structure.
- Upon loading into memory, these references form a fully interconnected graph, reflecting real-world entity relationships.

---

## **Philosophical Insights**

1. **Code Reflects a Vision**:
    - Every system represents a philosophy, offering a structured approach to addressing challenges.

2. **Interaction Drives Progress**:
    - Events and their consequences are the catalysts for change, development, and adaptation.

3. **Consistency and Scalability**:
    - Repetition and consistency of actions are essential for scaling and sustaining success.

4. **Holistic System Design**:
    - By structuring objects hierarchically, aligning internal and external dynamics, and emphasizing time-based insights, Yuneta fosters scalable and adaptive systems.

---

## **Conclusion**
Yuneta’s philosophy blends the technical and the philosophical, encouraging developers to think beyond code. It emphasizes the power of events, hierarchical organization, and consistent action while integrating time and data as essential dimensions for understanding and building interconnected systems.
