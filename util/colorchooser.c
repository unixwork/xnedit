/*
 * Copyright 2021 Olaf Wintermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include "colorchooser.h"

#include <Xm/XmAll.h>
#include <Xm/XmP.h>
#include <X11/Xft/Xft.h>

#include <inttypes.h>
#include <limits.h>

#define XNE_COLOR_DIALOG_TITLE "Select Color"

#define WIDGET_SPACING 5
#define WINDOW_SPACING 8

typedef struct {
    Widget dialog;
    
    Widget preview;
    Widget textfield;
    Widget selector;
    GC gc;
    GC selGC;
    XftDraw *d; // preview xft drawable
    
    uint8_t base_red;
    uint8_t base_green;
    uint8_t base_blue;
    
    int base_sel_y;
    float hue;
    float saturation;
    float value;
    
    uint8_t selected_red;
    uint8_t selected_green;
    uint8_t selected_blue;
    XftColor selected_color;
    
    XImage *image1;
    Dimension img1_width;
    Dimension img1_height;
    
    XImage *image2;
    Dimension img2_width;
    Dimension img2_height;
    
    int has_selection;
    Dimension img2_select_x;
    Dimension img2_select_y;
    
    int parse_textfield_input;
    
    int status;
    int end;
} cgData;

static XftDraw* create_xft_draw(Widget w);
static void selector_expose(Widget w, XtPointer u, XtPointer c);
static void selector_input(Widget w, XtPointer u, XtPointer c);
static void preview_color(Widget w, XtPointer u, XtPointer c);
static void textfield_changed(Widget w, XtPointer u, XtPointer c);
static void set_base_color(cgData *data, int r, int g, int b);
static void select_color(cgData *data, int r, int g, int b);

static void draw_img2(Display *dp, Window win, cgData *data);

static void okCB(Widget w, XtPointer u, XtPointer c);
static void cancelCB(Widget w, XtPointer u, XtPointer c);

int ColorChooser(Widget parent, int *red, int *green, int *blue) {
    Arg args[32];
    int n;
    XmString str;
    
    cgData data;
    memset(&data, 0, sizeof(data));
    
    data.base_sel_y = -1;
    
    n = 0;
    XtSetArg(args[n], XmNautoUnmanage, False); n++;
    Widget form = XmCreateFormDialog(parent, "ColorDialog", args, n);
    Widget dialog = XtParent(form);
    XtVaSetValues(dialog, XmNtitle, XNE_COLOR_DIALOG_TITLE, NULL);
    XtManageChild(form);
    
    // UI
    
    // color preview field
    n = 0;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 30); n++;
    //XtSetArg(args[n], XmNwidth, 50); n++;
    data.preview = XmCreateDrawingArea(form, "cgPreview", args, n);
    XtManageChild(data.preview);
    XtAddCallback(data.preview, XmNexposeCallback, preview_color, &data);
    
    // rgb textfield
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 33); n++;
    data.textfield = XmCreateTextField(form, "cgText", args, n);
    XtManageChild(data.textfield);
    XtAddCallback(data.textfield, XmNvalueChangedCallback, textfield_changed, &data);
    
    XtVaSetValues(data.preview, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, data.textfield, NULL);
    
    
    // ok/cancel buttons
    n = 0;
    str = XmStringCreateSimple("OK");
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 12); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 37); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget ok = XmCreatePushButton(form, "cgOK", args, n);
    XtManageChild(ok);
    XmStringFree(str);
    XtAddCallback(ok, XmNactivateCallback, okCB, &data);
    
    n = 0;
    str = XmStringCreateSimple("Cancel");
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 63); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 88); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget cancel = XmCreatePushButton(form, "cgOK", args, n);
    XtManageChild(cancel);
    XmStringFree(str);
    XtAddCallback(cancel, XmNactivateCallback, cancelCB, &data);
    
    XtVaSetValues(form, XmNdefaultButton, ok, NULL);
    XtVaSetValues(form, XmNcancelButton, cancel, NULL);
    
    // color selector
    static XtTranslations selectorTranslations = NULL;
    if(!selectorTranslations) {
        selectorTranslations = XtParseTranslationTable("<Btn1Down>: DrawingAreaInput() ManagerGadgetArm()\n<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()");
    }
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, data.textfield); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, ok); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNwidth, 500); n++;
    XtSetArg(args[n], XmNheight, 400); n++;
    XtSetArg(args[n], XmNtranslations, selectorTranslations); n++;
    data.selector = XmCreateDrawingArea(form, "cgSelector", args, n);    
    XtManageChild(data.selector);
    XtAddCallback(data.selector, XmNexposeCallback, selector_expose, &data);
    XtAddCallback(data.selector, XmNinputCallback, selector_input, &data);
    
    XGCValues gcValues;
    gcValues.background = data.selector->core.background_pixel;
    gcValues.foreground = BlackPixelOfScreen(DefaultScreenOfDisplay(XtDisplay(data.selector)));
    gcValues.graphics_exposures = 0;
    data.gc = XtAllocateGC(data.selector, 0, GCForeground | GCBackground, &gcValues, 0, 0);
    
    gcValues.background = data.selector->core.background_pixel;
    gcValues.foreground = WhitePixelOfScreen(DefaultScreenOfDisplay(XtDisplay(data.selector)));
    gcValues.graphics_exposures = 0;
    data.selGC = XtAllocateGC(data.selector, 0, GCForeground | GCBackground, &gcValues, 0, 0);
    
    // initial color
    select_color(&data, *red / 257, *green / 257, *blue / 257);
    
    // base color
    set_base_color(&data, (*red)/257, (*green)/257, (*blue)/257);
     
    // manage dialog
    XtManageChild(dialog);
    
    data.parse_textfield_input = 1;
    
    XtAppContext app = XtWidgetToApplicationContext(dialog);
    while(!data.end && !XtAppGetExitFlag(app)) {
        XEvent event;
        XtAppNextEvent(app, &event);
        XtDispatchEvent(&event);
    }
    
    XtReleaseGC(data.selector, data.gc);
    XtReleaseGC(data.selector, data.selGC);
    if(data.image1) XDestroyImage(data.image1);
    if(data.image2) XDestroyImage(data.image2);
    if(data.d) XftDrawDestroy(data.d);
    XtDestroyWidget(dialog);
    
    if(data.status == 1) {
        *red = data.selected_color.color.red;
        *green = data.selected_color.color.green;
        *blue = data.selected_color.color.blue;
    }
    
    return data.status;
}

static XftDraw* create_xft_draw(Widget w) {   
    XWindowAttributes attributes;
    XGetWindowAttributes(XtDisplay(w), XtWindow(w), &attributes); 
    
    Screen *screen = w->core.screen;
    Visual *visual = screen->root_visual;
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == w->core.depth) {
            visual = d.visuals;
            break;
        }
    }
    
    Display *dp = XtDisplay(w);
    return XftDrawCreate(
            dp,
            XtWindow(w),
            visual,
            w->core.colormap);
    
}

/*
 * get number of shifts for specific color mask
 */
static int get_shift(unsigned long mask) {
    if(mask == 0) {
        return 0;
    }
    
    int shift = 0;
    while((mask & 1L) == 0) {
        shift++;
        mask >>= 1;
    }
    return shift;
}

/*
 * get number of bits from color mask
 */
static int get_mask_len(unsigned long mask) {
    if(mask == 0) {
        return 0;
    }
    
    while((mask & 1L) == 0) {
        mask >>= 1;
    }
    int len = 0;
    while((mask & 1L) == 1) {
        len++;
        mask >>= 1;
    }
    return len;
}

static Visual* get_visual(Screen *screen, int depth) {
    // get the correct visual for current screen/depth
    Visual *visual = NULL;
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == depth) {
            for(int v=0;v<d.nvisuals;v++) {
                Visual *vs = &d.visuals[v];
                if(get_mask_len(vs->red_mask) == 8) {
                    visual = vs;
                    break;
                }
            }
        }
    }
    return visual;
}

#define IMG1_WIDTH    20
#define IMG1_X_OFFSET 20
#define IMG1_Y_OFFSET 10
#define IMG2_X_OFFSET 10
#define IMG2_Y_OFFSET 10

static void init_pix1(cgData *data, Widget w) {
    Display *dp = XtDisplay(w);
    Dimension width = IMG1_WIDTH;
    Dimension height = w->core.height - IMG1_Y_OFFSET*2;
    
    if(data->image1) {
        if(height == data->img2_height) return;
        XDestroyImage(data->image1);
    }
    
    float height6 = ((float)height)/6.f;
    float step = ((float)255.f)/height6;
    
    Visual *visual = get_visual(w->core.screen, w->core.depth);
    if(!visual) {
        return;
    }
    
    //get number of shifts required for each color bit mask
    int red_shift = get_shift(visual->red_mask);
    int green_shift = get_shift(visual->green_mask);
    int blue_shift = get_shift(visual->blue_mask);
    
    uint32_t *imgdata = malloc(width * height * 4); // will be freed by XDestroyImage
    uint32_t pixel_init = UINT32_MAX ^ (visual->red_mask ^ visual->green_mask ^ visual->blue_mask);

    
    int m = 0;
    float r = 0xFF;
    float g = 0;
    float b = 0;
    for(int i=0;i<height;i++) {
        if(r < 0) r = 0;
        if(r > 255) r = 255;
        if(g < 0) g = 0;
        if(g > 255) g = 255;
        if(b < 0) b = 0;
        if(b > 255) b = 255;
        
        uint8_t red = r;
        uint8_t green = g;
        uint8_t blue = b;
        
        switch(m) {
            case 0: {
                g += step;
                if(g >= 255) m++;
                break;
            }
            case 1: {
                r -= step;
                if(r <= 0) m++;
                break;
            }
            case 2: {
                b += step;
                if(b >= 255) m++;
                break;
            }
            case 3: {
                g -= step;
                if(g <= 0) m++;
                break;
            }
            case 4: {
                r += step;
                if(r >= 255) m++;
                break;
            }
            case 5: {
                b -= step;
                if(b <= 0) m = 0;
                break;
            }
        }
        
        uint32_t out = pixel_init;
        out ^= (red << red_shift);
        out ^= (green << green_shift);
        out ^= (blue << blue_shift);
        
        for(int j=0;j<width;j++) {
            imgdata[i*width + j] = out;
        }
    }
    
    data->image1 = XCreateImage(dp, visual, w->core.depth, ZPixmap, 0, (char*)imgdata, width, height, 32, 0);
    
    data->img1_width = width;
    data->img1_height = height;
}

static void init_pix2(cgData *data, Widget w) {
    Display *dp = XtDisplay(w);
    Dimension width = w->core.width - IMG1_X_OFFSET - IMG1_WIDTH - 2*IMG2_X_OFFSET;
    Dimension height = w->core.height - 2*IMG2_Y_OFFSET;
    
    if(data->image2) {
        if(width == data->img2_width == height == data->img2_height) return;
        XDestroyImage(data->image2);
        data->has_selection = 0;
    }
    
    if(!data->has_selection) {
        data->img2_select_x = (float)width * data->value - 1;
        data->img2_select_y = height - ((float)height * data->saturation);
        data->has_selection = 1;
    }
    
    Visual *visual = get_visual(w->core.screen, w->core.depth);
    if(!visual) {
        return;
    }
    
    //get number of shifts required for each color bit mask
    int red_shift = get_shift(visual->red_mask);
    int green_shift = get_shift(visual->green_mask);
    int blue_shift = get_shift(visual->blue_mask);
    
    uint32_t *imgdata = malloc(width * height * 4);
    uint32_t pixel_init = UINT32_MAX ^ (visual->red_mask ^ visual->green_mask ^ visual->blue_mask);
    
    float y_base_r = data->base_red;
    float y_base_g = data->base_green;
    float y_base_b = data->base_blue;
    
    float y_step_r = (255-data->base_red) / (float)(height-2*IMG2_Y_OFFSET);
    float y_step_g = (255-data->base_green) / (float)(height-2*IMG2_Y_OFFSET);
    float y_step_b = (255-data->base_blue) / (float)(height-2*IMG2_Y_OFFSET);
    
    for(int y=0;y<height;y++) { 
        float x_step_r = y_base_r / (float)width;
        float x_step_g = y_base_g / (float)width;
        float x_step_b = y_base_b / (float)width;
        
        float r = y_base_r;
        float g = y_base_g;
        float b = y_base_b;
        
        y_base_r += y_step_r;
        y_base_g += y_step_g;
        y_base_b += y_step_b;
        if(y_base_r >= 255) y_base_r = 255;
        if(y_base_g >= 255) y_base_g = 255;
        if(y_base_b >= 255) y_base_b = 255;
        
        for(int x=width-1;x>=0;x--) {
            if(r < 0) r = 0;
            if(g < 0) g = 0;
            if(b < 0) b = 0;
            
            uint8_t red = r;
            uint8_t green = g;
            uint8_t blue = b;
            
            r -= x_step_r;
            g -= x_step_g;
            b -= x_step_b;
            
            uint32_t out = pixel_init;
            out ^= (red << red_shift);
            out ^= (green << green_shift);
            out ^= (blue << blue_shift);
            imgdata[y*width + x] = out;
        }
    }
    
    data->image2 = XCreateImage(dp, visual, w->core.depth, ZPixmap, 0, (char*)imgdata, width, height, 32, 0);
    
    data->img2_width = width;
    data->img2_height = height;
}

static void draw_img2(Display *dp, Window win, cgData *data) {
    int img2_x = IMG1_X_OFFSET + data->img1_width + IMG2_X_OFFSET;
    int img2_y = IMG2_Y_OFFSET;
    XPutImage(dp, win, data->gc, data->image2, 0, 0, img2_x, img2_y, data->img2_width, data->img2_height);
    if(data->has_selection) {
        XDrawLine(dp, win, data->selGC,
                img2_x, img2_y + data->img2_select_y,
                img2_x + data->img2_width, img2_y + data->img2_select_y);
        XDrawLine(dp, win, data->selGC,
                img2_x + data->img2_select_x, img2_y,
                img2_x + data->img2_select_x, img2_y + data->img2_height - 1);
    }
}

#define HUE_INDICATOR_SIZE 9

static void selector_expose(Widget w, XtPointer u, XtPointer c) {
    cgData *data = u;
    Dimension width = w->core.width;
    Dimension height = w->core.height;
    Display *dp = XtDisplay(w);
    Window win = XtWindow(w);
    
    init_pix1(data, w);
    init_pix2(data, w);
    
    if(data->base_sel_y < 0) {
        float img1h = data->img1_height;
        float deg2pix = img1h / 360.f;
        data->base_sel_y = data->hue * deg2pix;
    }
    
    // clear all
    
    // top
    XClearArea(XtDisplay(w), XtWindow(w), 1, 1, width-2, IMG1_Y_OFFSET-1, False);
    // bottom
    XClearArea(XtDisplay(w), XtWindow(w),
            1, IMG1_Y_OFFSET + data->img1_height,
            width-2, height - IMG1_Y_OFFSET - data->img1_height - 2, False);
    // left
    XClearArea(XtDisplay(w), XtWindow(w), 1, 1, IMG1_X_OFFSET-1, height-2, False);

    XPoint indicator[4];
    indicator[0].x = IMG1_X_OFFSET-1;
    indicator[0].y = IMG1_Y_OFFSET + data->base_sel_y;

    indicator[1].x = IMG1_X_OFFSET-HUE_INDICATOR_SIZE - 1;
    indicator[1].y = IMG1_Y_OFFSET + data->base_sel_y - HUE_INDICATOR_SIZE;

    indicator[2].x = IMG1_X_OFFSET-HUE_INDICATOR_SIZE - 1;
    indicator[2].y = IMG1_Y_OFFSET + data->base_sel_y + HUE_INDICATOR_SIZE;

    indicator[3].x = IMG1_X_OFFSET-1;
    indicator[3].y = IMG1_Y_OFFSET + data->base_sel_y;


    XFillPolygon(dp, win, data->gc, indicator, 4, Convex, CoordModeOrigin);
    
    // right
    XClearArea(XtDisplay(w), XtWindow(w),
            IMG1_X_OFFSET + data->img1_width + IMG2_X_OFFSET + data->img2_width, 1,
            width - 1 - IMG1_X_OFFSET - data->img1_width - IMG2_X_OFFSET - data->img2_width, height-2, False);
    // middle
    XClearArea(XtDisplay(w), XtWindow(w),
            IMG1_X_OFFSET + data->img1_width, 1,
            IMG2_X_OFFSET, height-2, False);
    
    
    if(data->image1) {
        XPutImage(dp, win, data->gc, data->image1, 0, 0, IMG1_X_OFFSET, IMG1_Y_OFFSET, data->img1_width, data->img1_height);
    }
    if(data->image2) {
        draw_img2(dp, win, data);
    }
    
    XDrawRectangle(dp, win, DefaultGC(dp, 0), 0, 0, width-1, height-1);
    
}

#define SELECT_MARGIN 5
static int translate_img1(cgData *data, int x, int y, int *trans_x, int *trans_y) {
    if(x < IMG1_X_OFFSET - HUE_INDICATOR_SIZE) return 0;
    if(y < IMG1_Y_OFFSET) return 0;
    
    int tx = x - IMG1_X_OFFSET;
    int ty = y - IMG1_Y_OFFSET;
    
    if(tx < data->img1_width && ty < data->img1_height) {
        *trans_x = tx;
        *trans_y = ty;
        return 1;
    }
    return 0;
}

static int translate_img2(cgData *data, int x, int y, int *trans_x, int *trans_y) {
    if(x < IMG1_X_OFFSET + IMG2_X_OFFSET + data->img1_width - SELECT_MARGIN) return 0;
    if(y < IMG1_Y_OFFSET) return 0;
    
    int tx = x - (IMG1_X_OFFSET + IMG2_X_OFFSET + data->img1_width);
    int ty = y - IMG2_Y_OFFSET;
    
    if(tx < data->img2_width + SELECT_MARGIN && ty < data->img2_height + SELECT_MARGIN) {
        if(tx < 0) tx = 0;
        if(ty < 0) ty = 0;
        if(tx >= data->img2_width) tx = data->img2_width-1;
        if(ty >= data->img2_height) ty = data->img2_height-1;
        *trans_x = tx;
        *trans_y = ty;
        return 1;
    }
    return 0;
}

static void selector_input(Widget w, XtPointer u, XtPointer c) {
    XmDrawingAreaCallbackStruct *cb = (XmDrawingAreaCallbackStruct*)c;
    cgData *data = u;
    Display *dp = XtDisplay(w);
    Window win = XtWindow(w);

    Dimension x, y;
    if(cb->event->type == MotionNotify) {
        x = cb->event->xmotion.x;
        y = cb->event->xmotion.y;
    } else {
        x = cb->event->xbutton.x;
        y = cb->event->xbutton.y;
    }
    
    int tx, ty;
    if(translate_img1(data, x, y, &tx, &ty)) {
        uint32_t color = XGetPixel(data->image1, tx, ty);
        int red_shift = get_shift(data->image1->red_mask);
        int green_shift = get_shift(data->image1->green_mask);
        int blue_shift = get_shift(data->image1->blue_mask);
        
        data->base_red = (color & data->image1->red_mask) >> red_shift;
        data->base_green = (color & data->image1->green_mask) >> green_shift;
        data->base_blue = (color & data->image1->blue_mask) >> blue_shift;
        
        data->base_sel_y = ty;
        
        XDestroyImage(data->image2);
        data->image2 = NULL;
        init_pix2(data, w);
        
        data->has_selection = 0;
        selector_expose(w, u, NULL);
    }
    if(translate_img2(data, x, y, &tx, &ty)) {
        uint32_t color = XGetPixel(data->image2, tx, ty);
        int red_shift = get_shift(data->image2->red_mask);
        int green_shift = get_shift(data->image2->green_mask);
        int blue_shift = get_shift(data->image2->blue_mask);
        
        data->img2_select_x = tx;
        data->img2_select_y = ty;
        data->has_selection = 1;
        
        select_color(data,
                (color & data->image2->red_mask) >> red_shift,
                (color & data->image2->green_mask) >> green_shift,
                (color & data->image2->blue_mask) >> blue_shift);
        
        preview_color(data->preview, data, NULL);
        
        draw_img2(dp, win, data);
    }
}

static float max3(float a, float b, float c) {
    if(a > b) {
        return a > c ? a : c;
    } else {
        return b > c ? b : c;
    }
}

static float min3(float a, float b, float c) {
    if(a < b) {
        return a < c ? a : c;
    } else {
        return b < c ? b : c;
    }
}

static void rgbToHsv(int red, int green, int blue, float *hue, float *saturation, float *value) {
    float r = red;
    float g = green;
    float b = blue;
    r /= 255;
    g /= 255;
    b /= 255;
    
    float rgbMax = max3(r, g, b);
    float rgbMin = min3(r, g, b);
    
    float mmDiff = rgbMax - rgbMin;
    
    float h;
    if(rgbMax == rgbMin) {
        h = 0;
    } else if(rgbMax == r) {
        h = 60.f * (g-b)/mmDiff;
    } else if(rgbMax == g) {
        h = 60.f * (2.f + (b-r)/mmDiff);
    } else {
        h = 60.f * (4.f + (r-g)/mmDiff);
    }
    if(h < 0) h += 360;
    
    float s = rgbMax == 0 ? 0 : mmDiff/rgbMax;
    float v = rgbMax;
    
    *hue = h;
    *saturation = s;
    *value = v;
}

static void hsvToRgb(float hue, float saturation, float value, int *red, int *green, int *blue) {
    int hi = hue/60;
    float f = (hue/60) - hi;
    float p = value * (1-saturation);
    float q = value * (1-saturation * f);
    float t = value * (1-saturation * (1-f));
    
    float r, g, b;
    switch(hi) {
        case 1: {
            r = q;
            g = value;
            b = p;
            break;
        }
        case 2: {
            r = p;
            g = value;
            b = t;
            break;
        }
        case 3: {
            r = p;
            g = q;
            b = value;
            break;
        }
        case 4: {
            r = t;
            g = p;
            b = value;
            break;
        }
        case 5: {
            r = value;
            g = p;
            b = q;
            break;  
        }
        default: {
            r = value;
            g = t;
            b = p;
        }
    }
    
    *red = r * 255;
    *green = g * 255;
    *blue = b * 255;
}

static void set_base_color(cgData *data, int r, int g, int b) {
    float h, s, v;
    rgbToHsv(r, g, b, &h, &s, &v);
       
    int red, green, blue;
    hsvToRgb(h, 1, 1, &red, &green, &blue);
    
    data->base_red = red;
    data->base_green = green;
    data->base_blue = blue;
    data->hue = h;
    data->saturation = s;
    data->value = v;
    data->base_sel_y = -1;
}

static void select_color(cgData *data, int r, int g, int b) {
    XftColor color;
    color.pixel = 0;
    color.color.alpha = 0xFFFF;
    color.color.red = r * 257;
    color.color.green = g * 257;
    color.color.blue = b * 257;
    data->selected_color = color;
    
    char buf[8];
    snprintf(buf, 8, "#%02x%02x%02x", r, g, b);
    data->parse_textfield_input = 0;
    XmTextFieldSetString(data->textfield, buf);
    data->parse_textfield_input = 1;
}

static void preview_color(Widget w, XtPointer u, XtPointer c) {
    cgData *data = u;
    if(!data->d) {
        data->d = create_xft_draw(w);
    }
    
    XftDrawRect(data->d, &data->selected_color, 0, 0, w->core.width, w->core.height);
    XDrawRectangle(XtDisplay(w), XtWindow(w), data->gc, 0, 0, w->core.width-1, w->core.height-1);
}

static void textfield_changed(Widget w, XtPointer u, XtPointer c) {
    cgData *data = u;
    if(!data->parse_textfield_input) return;
    
    char *colorStr = XmTextFieldGetString(w);
    if(strlen(colorStr) != 7) {
        XtFree(colorStr);
        return;
    }
    
    Display *dp = XtDisplay(w);
    Colormap cmap = w->core.colormap;
    XColor color;
    if(XParseColor(dp, cmap, colorStr, &color)) {
        XftColor c;
        c.pixel = 0;
        c.color.alpha = 0xFFFF;
        c.color.red = color.red;
        c.color.green = color.green;
        c.color.blue = color.blue;
        data->selected_color = c;
        
        set_base_color(data, color.red/257, color.green/257, color.blue/257);
        
        XDestroyImage(data->image2);
        data->image2 = NULL;
        init_pix2(data, w);
        
        data->has_selection = 0;
        
        selector_expose(data->selector, data, NULL);
        preview_color(data->preview, data, NULL);
    }
    XtFree(colorStr);
}

static void okCB(Widget w, XtPointer u, XtPointer c) {
    cgData *data = u;
    data->status = 1;
    data->end = 1;
}

static void cancelCB(Widget w, XtPointer u, XtPointer c) {
    cgData *data = u;
    data->status = 0;
    data->end = 1;
}
