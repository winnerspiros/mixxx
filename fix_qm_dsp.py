with open('lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp', 'r') as f:
    lines = f.readlines()

for i in range(len(lines)):
    if 'for (int j = 20; j < Q - 20; j++)' in lines[i]:
        lines[i] = lines[i].replace('int j = 20', 'std::size_t j = 20')

with open('lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp', 'w') as f:
    f.writelines(lines)
