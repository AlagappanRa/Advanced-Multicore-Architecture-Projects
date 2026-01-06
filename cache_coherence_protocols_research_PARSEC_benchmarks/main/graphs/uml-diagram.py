from graphviz import Digraph

def generate_architecture_diagram():
    # SETTING 1: Anti-aliasing and High DPI for crisp text
    dot = Digraph(comment='Coherence Simulator Architecture')
    dot.attr(dpi='300')              # High resolution
    dot.attr(rankdir='TB')           # Top-to-Bottom layout
    dot.attr(newrank='true')         # Better alignment across clusters
    
    # SETTING 2: Use a native Windows font (Arial or Segoe UI)
    # 'penwidth' makes the boxes look less "sketchy"
    dot.attr('node', shape='record', style='filled', 
             fillcolor='#f0f0f0', fontname="Arial", fontsize="10", penwidth="0.5")
    dot.attr('edge', fontname="Arial", fontsize="9")
    dot.attr('graph', fontname="Arial", fontsize="10")

    # 1. The Core Engine
    dot.node('Sim', r'{Simulator| + run() \n + schedule(Event) \n + print_stats()}', fillcolor='#D4E6F1')
    dot.node('Queue', r'{EventQueue| - std::priority_queue \n - current_time \n | + pop() \n + push()}', fillcolor='#D4E6F1')
    
    # 2. Hardware Components
    with dot.subgraph(name='cluster_HW') as c:
        c.attr(label='Hardware Models', color='grey', style='dashed', fontcolor='grey')
        c.node('Bus', r'{BusResource| - wait_queue \n - busy_flag | + arbitration()}', fillcolor='#FCF3CF')
        c.node('Core', r'{CoreState| - blocked_flag \n - compute_cycles}', fillcolor='#FCF3CF')
        c.node('Stats', r'{Statistics| - hits/misses \n - traffic \n - invalidations}', fillcolor='#FCF3CF')
    
    # 3. Protocol Logic (Strategy Pattern)
    with dot.subgraph(name='cluster_Proto') as c:
        c.attr(label='Coherence Logic (Strategy)', color='blue', fontcolor='blue')
        c.node('Base', r'{ \<\<Abstract\>\>\nCoherenceProtocol| + handle_request() \n + handle_bus_tx()}', fillcolor='#D5F5E3')
        c.node('MESI', '{MESIProtocol}', fillcolor='#ffffff')
        c.node('Dragon', '{DragonProtocol}', fillcolor='#ffffff')
        c.node('MESIF', '{MESIFProtocol}', fillcolor='#ffffff')

    # 4. Events (Command Pattern)
    with dot.subgraph(name='cluster_Events') as c:
        c.attr(label='Discrete Events', color='red', fontcolor='red')
        c.node('Event', r'{ \<\<Abstract\>\>\nEvent| + timestamp \n + core_id \n | + execute(Sim)}', fillcolor='#FADBD8')
        c.node('Fetch', 'FetchEvent', fillcolor='#ffffff')
        c.node('Arb', 'BusArbitrationEvent', fillcolor='#ffffff')
        c.node('Rel', 'BusReleaseEvent', fillcolor='#ffffff')
        c.node('Unblock', 'CoreUnblockEvent', fillcolor='#ffffff')

    # Relationships
    dot.edge('Sim', 'Queue', label=' owns')
    dot.edge('Sim', 'Bus', label=' manages')
    dot.edge('Sim', 'Base', label=' delegates logic')
    
    # Protocol Inheritance
    dot.edge('Base', 'MESI', arrowtail='onormal', dir='back')
    dot.edge('Base', 'Dragon', arrowtail='onormal', dir='back')
    dot.edge('Base', 'MESIF', arrowtail='onormal', dir='back')

    # Event Inheritance
    dot.edge('Event', 'Fetch', arrowtail='onormal', dir='back')
    dot.edge('Event', 'Arb', arrowtail='onormal', dir='back')
    dot.edge('Event', 'Rel', arrowtail='onormal', dir='back')
    dot.edge('Event', 'Unblock', arrowtail='onormal', dir='back')

    # Execution flow
    dot.edge('Queue', 'Event', label=' stores', style='dashed', color='#555555')
    dot.edge('Event', 'Sim', label=' modifies state', style='dotted', color='#555555')

    try:
        # Rendering both PNG (High Res) and SVG (Vector)
        dot.render('architecture_diagram', format='png', cleanup=True)
        dot.render('architecture_diagram_vector', format='svg', cleanup=True)
        print("Diagram generated: architecture_diagram.png (High Res) and .svg (Vector)")
    except Exception as e:
        print(f"Error generating diagram: {e}")

if __name__ == '__main__':
    generate_architecture_diagram()
