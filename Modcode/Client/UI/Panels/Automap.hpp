#pragma once
#include "../D2Panel.hpp"

#define AUTOMAP_MAX_LINES 5

class Automap : public D2Panel
{
private:
    IRenderObject *m_lines[AUTOMAP_MAX_LINES];

public:
    Automap();
    ~Automap();

    void Draw() override;
    void Tick(DWORD dwDeltaMs) override;
};
