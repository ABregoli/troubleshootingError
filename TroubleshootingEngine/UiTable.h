#pragma once 

#include <FL/Fl_Table_Row.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <vector>
#include <string>

#include "SqlObj.h"


class ProcedureTable : public Fl_Table_Row
{
    std::vector<ProcedureStep> m_steps;

public:

ProcedureTable(int X, int Y, int W, int H) : Fl_Table_Row(X, Y, W, H)
{
    cols(5);

    col_width(0, 60);
    col_width(1, 200);
    col_width(2, 500);
    col_width(3, 100);
    col_width(4, 150);

    col_header(1);
    row_header(0);

    end();
}

void setSteps(const std::vector<ProcedureStep>& steps)
{
    m_steps = steps;
    rows(static_cast<int>(steps.size()));
    redraw();
}

private:

void draw_cell(TableContext context, int row, int col, int x, int y, int w, int h) override
{
    switch(context){
        case CONTEXT_COL_HEADER:
        {
            fl_push_clip(x,y,w,h);
            fl_draw_box(FL_THIN_UP_BOX,x,y,w,h,FL_LIGHT2);

            static const char* headers[] =
            {
                "Ord.",
                "Titolo",
                "Istruzione",
                "Escalation",
                "Ruolo"
            };

            fl_color(FL_BLACK);
            fl_draw(headers[col],x,y,w,h,FL_ALIGN_CENTER);
            fl_pop_clip();

            break;
        }
        case CONTEXT_CELL:
        {
            fl_push_clip(x, y, w, h);

            fl_color(FL_WHITE);
            fl_rectf(x, y, w, h);

            fl_color(FL_GRAY);
            fl_rect(x, y, w, h);

            std::string text;

            switch(col)
            {
            case 0: text = std::to_string(m_steps[row].order); break;
            case 1: text = m_steps[row].title; break;
            case 2: text = m_steps[row].instruction; break;
            case 3: text = m_steps[row].escalationRequired ? "SI" : "NO"; break;
            case 4: text = m_steps[row].escalationRole; break;
            }

            fl_color(FL_BLACK);
            fl_draw(text.c_str(), x + 4, y, w - 8, h, FL_ALIGN_LEFT);
            fl_pop_clip();

            break;
        }
        default:
        {
            break;
        }
    }
}
};