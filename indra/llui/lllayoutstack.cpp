/** 
 * @file lllayoutstack.cpp
 * @brief LLLayout class - dynamic stacking of UI elements
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

// Opaque view with a background and a border.  Can contain LLUICtrls.

#include "linden_common.h"

#include "lllayoutstack.h"

#include "lllocalcliprect.h"
#include "llpanel.h"
#include "llresizebar.h"
#include "llcriticaldamp.h"

static LLDefaultChildRegistry::Register<LLLayoutStack> register_layout_stack("layout_stack");
static LLLayoutStack::LayoutStackRegistry::Register<LLLayoutPanel> register_layout_panel("layout_panel");

//
// LLLayoutPanel
//
LLLayoutPanel::LLLayoutPanel(const Params& p)	
:	LLPanel(p),
 	mMinDim(p.min_dim), 
 	mMaxDim(p.max_dim), 
 	mAutoResize(p.auto_resize),
 	mUserResize(p.user_resize),
 	mCollapsed(FALSE),
 	mCollapseAmt(0.f),
 	mVisibleAmt(1.f), // default to fully visible
 	mResizeBar(NULL) 
{
	// panels initialized as hidden should not start out partially visible
	if (!getVisible())
	{
		mVisibleAmt = 0.f;
	}
}

LLLayoutPanel::~LLLayoutPanel()
{
	// probably not necessary, but...
	delete mResizeBar;
	mResizeBar = NULL;
}
	
F32 LLLayoutPanel::getCollapseFactor(LLLayoutStack::ELayoutOrientation orientation)
{
	if (orientation == LLLayoutStack::HORIZONTAL)
	{
		F32 collapse_amt = 
			clamp_rescale(mCollapseAmt, 0.f, 1.f, 1.f, (F32)mMinDim / (F32)llmax(1, getRect().getWidth()));
		return mVisibleAmt * collapse_amt;
	}
	else
	{
		F32 collapse_amt = 
			clamp_rescale(mCollapseAmt, 0.f, 1.f, 1.f, llmin(1.f, (F32)mMinDim / (F32)llmax(1, getRect().getHeight())));
		return mVisibleAmt * collapse_amt;
	}
}

//
// LLLayoutStack
//

LLLayoutStack::Params::Params()
:	orientation("orientation", std::string("vertical")),
	animate("animate", true),
	clip("clip", true),
	border_size("border_size", LLCachedControl<S32>(*LLUI::sSettingGroups["config"], "UIResizeBarHeight", 0))
{
	name="stack";
}

LLLayoutStack::LLLayoutStack(const LLLayoutStack::Params& p) 
:	LLView(p),
	mMinWidth(0),
	mMinHeight(0),
	mPanelSpacing(p.border_size),
	mOrientation((p.orientation() == "vertical") ? VERTICAL : HORIZONTAL),
	mAnimate(p.animate),
	mAnimatedThisFrame(false),
	mClip(p.clip)
{}

LLLayoutStack::~LLLayoutStack()
{
	e_panel_list_t panels = mPanels; // copy list of panel pointers
	mPanels.clear(); // clear so that removeChild() calls don't cause trouble
	std::for_each(panels.begin(), panels.end(), DeletePointer());
}

void LLLayoutStack::draw()
{
	updateLayout();

	e_panel_list_t::iterator panel_it;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		// clip to layout rectangle, not bounding rectangle
		LLRect clip_rect = (*panel_it)->getRect();
		// scale clipping rectangle by visible amount
		if (mOrientation == HORIZONTAL)
		{
			clip_rect.mRight = clip_rect.mLeft + llround((F32)clip_rect.getWidth() * (*panel_it)->getCollapseFactor(mOrientation));
		}
		else
		{
			clip_rect.mBottom = clip_rect.mTop - llround((F32)clip_rect.getHeight() * (*panel_it)->getCollapseFactor(mOrientation));
		}

		LLPanel* panelp = (*panel_it);

		LLLocalClipRect clip(clip_rect, mClip);
		// only force drawing invisible children if visible amount is non-zero
		drawChild(panelp, 0, 0, !clip_rect.isEmpty());
	}
	mAnimatedThisFrame = false;
}

void LLLayoutStack::removeChild(LLView* view)
{
	LLLayoutPanel* embedded_panelp = findEmbeddedPanel(dynamic_cast<LLPanel*>(view));

	if (embedded_panelp)
	{
		mPanels.erase(std::find(mPanels.begin(), mPanels.end(), embedded_panelp));
		delete embedded_panelp;
	}

	// need to update resizebars

	calcMinExtents();

	LLView::removeChild(view);
}

BOOL LLLayoutStack::postBuild()
{
	updateLayout();
	return TRUE;
}

bool LLLayoutStack::addChild(LLView* child, S32 tab_group)
{
	LLLayoutPanel* panelp = dynamic_cast<LLLayoutPanel*>(child);
	if (panelp)
	{
		mPanels.push_back(panelp);
	}
	return LLView::addChild(child, tab_group);
}


S32 LLLayoutStack::getDefaultHeight(S32 cur_height)
{
	// if we are spanning our children (crude upward propagation of size)
	// then don't enforce our size on our children
	if (mOrientation == HORIZONTAL)
	{
		cur_height = llmax(mMinHeight, getRect().getHeight());
	}

	return cur_height;
}

S32 LLLayoutStack::getDefaultWidth(S32 cur_width)
{
	// if we are spanning our children (crude upward propagation of size)
	// then don't enforce our size on our children
	if (mOrientation == VERTICAL)
	{
		cur_width = llmax(mMinWidth, getRect().getWidth());
	}

	return cur_width;
}

void LLLayoutStack::addPanel(LLLayoutPanel* panel, EAnimate animate)
{
	addChild(panel);

	// panel starts off invisible (collapsed)
	if (animate == ANIMATE)
	{
		panel->mVisibleAmt = 0.f;
		panel->setVisible(TRUE);
	}
}

void LLLayoutStack::removePanel(LLPanel* panel)
{
	removeChild(panel);
}

void LLLayoutStack::collapsePanel(LLPanel* panel, BOOL collapsed)
{
	LLLayoutPanel* panel_container = findEmbeddedPanel(panel);
	if (!panel_container) return;

	panel_container->mCollapsed = collapsed;
}

void LLLayoutStack::updatePanelAutoResize(const std::string& panel_name, BOOL auto_resize)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);

	if (panel)
	{
		panel->mAutoResize = auto_resize;
	}
}

void LLLayoutStack::setPanelUserResize(const std::string& panel_name, BOOL user_resize)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);

	if (panel)
	{
		panel->mUserResize = user_resize;
	}
}

bool LLLayoutStack::getPanelMinSize(const std::string& panel_name, S32* min_dimp)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);

	if (panel)
	{
		if (min_dimp) *min_dimp = panel->mMinDim;
	}

	return NULL != panel;
}

bool LLLayoutStack::getPanelMaxSize(const std::string& panel_name, S32* max_dimp)
{
	LLLayoutPanel* panel = findEmbeddedPanelByName(panel_name);

	if (panel)
	{
		if (max_dimp) *max_dimp = panel->mMaxDim;
	}

	return NULL != panel;
}

static LLFastTimer::DeclareTimer FTM_UPDATE_LAYOUT("Update LayoutStacks");
void LLLayoutStack::updateLayout(BOOL force_resize)
{
	LLFastTimer ft(FTM_UPDATE_LAYOUT);
	static LLUICachedControl<S32> resize_bar_overlap ("UIResizeBarOverlap", 0);
	calcMinExtents();
	createResizeBars();

	// calculate current extents
	S32 total_width = 0;
	S32 total_height = 0;

	const F32 ANIM_OPEN_TIME = 0.02f;
	const F32 ANIM_CLOSE_TIME = 0.03f;

	e_panel_list_t::iterator panel_it;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end();	++panel_it)
	{
		LLPanel* panelp = (*panel_it);
		if (panelp->getVisible()) 
		{
			if (mAnimate)
			{
				if (!mAnimatedThisFrame)
				{
					(*panel_it)->mVisibleAmt = lerp((*panel_it)->mVisibleAmt, 1.f, LLCriticalDamp::getInterpolant(ANIM_OPEN_TIME));
					if ((*panel_it)->mVisibleAmt > 0.99f)
					{
						(*panel_it)->mVisibleAmt = 1.f;
					}
				}
			}
			else
			{
				(*panel_it)->mVisibleAmt = 1.f;
			}
		}
		else // not visible
		{
			if (mAnimate)
			{
				if (!mAnimatedThisFrame)
				{
					(*panel_it)->mVisibleAmt = lerp((*panel_it)->mVisibleAmt, 0.f, LLCriticalDamp::getInterpolant(ANIM_CLOSE_TIME));
					if ((*panel_it)->mVisibleAmt < 0.001f)
					{
						(*panel_it)->mVisibleAmt = 0.f;
					}
				}
			}
			else
			{
				(*panel_it)->mVisibleAmt = 0.f;
			}
		}

		if ((*panel_it)->mCollapsed)
		{
			(*panel_it)->mCollapseAmt = lerp((*panel_it)->mCollapseAmt, 1.f, LLCriticalDamp::getInterpolant(ANIM_CLOSE_TIME));
		}
		else
		{
			(*panel_it)->mCollapseAmt = lerp((*panel_it)->mCollapseAmt, 0.f, LLCriticalDamp::getInterpolant(ANIM_CLOSE_TIME));
		}

		if (mOrientation == HORIZONTAL)
		{
			// enforce minimize size constraint by default
			if (panelp->getRect().getWidth() < (*panel_it)->mMinDim)
			{
				panelp->reshape((*panel_it)->mMinDim, panelp->getRect().getHeight());
			}
        	total_width += llround(panelp->getRect().getWidth() * (*panel_it)->getCollapseFactor(mOrientation));
        	// want n-1 panel gaps for n panels
			if (panel_it != mPanels.begin())
			{
				total_width += mPanelSpacing;
			}
		}
		else //VERTICAL
		{
			// enforce minimize size constraint by default
			if (panelp->getRect().getHeight() < (*panel_it)->mMinDim)
			{
				panelp->reshape(panelp->getRect().getWidth(), (*panel_it)->mMinDim);
			}
			total_height += llround(panelp->getRect().getHeight() * (*panel_it)->getCollapseFactor(mOrientation));
			if (panel_it != mPanels.begin())
			{
				total_height += mPanelSpacing;
			}
		}
	}

	S32 num_resizable_panels = 0;
	S32 shrink_headroom_available = 0;
	S32 shrink_headroom_total = 0;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		// panels that are not fully visible do not count towards shrink headroom
		if ((*panel_it)->getCollapseFactor(mOrientation) < 1.f) 
		{
			continue;
		}

		// if currently resizing a panel or the panel is flagged as not automatically resizing
		// only track total available headroom, but don't use it for automatic resize logic
		if ((*panel_it)->mResizeBar->hasMouseCapture() 
			|| (!(*panel_it)->mAutoResize 
				&& !force_resize))
		{
			if (mOrientation == HORIZONTAL)
			{
				shrink_headroom_total += (*panel_it)->getRect().getWidth() - (*panel_it)->mMinDim;
			}
			else //VERTICAL
			{
				shrink_headroom_total += (*panel_it)->getRect().getHeight() - (*panel_it)->mMinDim;
			}
		}
		else
		{
			num_resizable_panels++;
			if (mOrientation == HORIZONTAL)
			{
				shrink_headroom_available += (*panel_it)->getRect().getWidth() - (*panel_it)->mMinDim;
				shrink_headroom_total += (*panel_it)->getRect().getWidth() - (*panel_it)->mMinDim;
			}
			else //VERTICAL
			{
				shrink_headroom_available += (*panel_it)->getRect().getHeight() - (*panel_it)->mMinDim;
				shrink_headroom_total += (*panel_it)->getRect().getHeight() - (*panel_it)->mMinDim;
			}
		}
	}

	// calculate how many pixels need to be distributed among layout panels
	// positive means panels need to grow, negative means shrink
	S32 pixels_to_distribute;
	if (mOrientation == HORIZONTAL)
	{
		pixels_to_distribute = getRect().getWidth() - total_width;
	}
	else //VERTICAL
	{
		pixels_to_distribute = getRect().getHeight() - total_height;
	}

	// now we distribute the pixels...
	S32 cur_x = 0;
	S32 cur_y = getRect().getHeight();

	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLPanel* panelp = (*panel_it);

		S32 cur_width = panelp->getRect().getWidth();
		S32 cur_height = panelp->getRect().getHeight();
		S32 new_width = cur_width;
		S32 new_height = cur_height; 

		if (mOrientation == HORIZONTAL)
		{
			new_width = llmax((*panel_it)->mMinDim, new_width);
		}
		else
		{
			new_height = llmax((*panel_it)->mMinDim, new_height);
		}
		S32 delta_size = 0;

		// if panel can automatically resize (not animating, and resize flag set)...
		if ((*panel_it)->getCollapseFactor(mOrientation) == 1.f 
			&& (force_resize || (*panel_it)->mAutoResize) 
			&& !(*panel_it)->mResizeBar->hasMouseCapture()) 
		{
			if (mOrientation == HORIZONTAL)
			{
				// if we're shrinking
				if (pixels_to_distribute < 0)
				{
					// shrink proportionally to amount over minimum
					// so we can do this in one pass
					delta_size = (shrink_headroom_available > 0) ? llround((F32)pixels_to_distribute * ((F32)(cur_width - (*panel_it)->mMinDim) / (F32)shrink_headroom_available)) : 0;
					shrink_headroom_available -= (cur_width - (*panel_it)->mMinDim);
				}
				else
				{
					// grow all elements equally
					delta_size = llround((F32)pixels_to_distribute / (F32)num_resizable_panels);
					num_resizable_panels--;
				}
				pixels_to_distribute -= delta_size;
				new_width = llmax((*panel_it)->mMinDim, cur_width + delta_size);
			}
			else
			{
				new_width = getDefaultWidth(new_width);
			}

			if (mOrientation == VERTICAL)
			{
				if (pixels_to_distribute < 0)
				{
					// shrink proportionally to amount over minimum
					// so we can do this in one pass
					delta_size = (shrink_headroom_available > 0) ? llround((F32)pixels_to_distribute * ((F32)(cur_height - (*panel_it)->mMinDim) / (F32)shrink_headroom_available)) : 0;
					shrink_headroom_available -= (cur_height - (*panel_it)->mMinDim);
				}
				else
				{
					delta_size = llround((F32)pixels_to_distribute / (F32)num_resizable_panels);
					num_resizable_panels--;
				}
				pixels_to_distribute -= delta_size;
				new_height = llmax((*panel_it)->mMinDim, cur_height + delta_size);
			}
			else
			{
				new_height = getDefaultHeight(new_height);
			}
		}
		else
		{
			if (mOrientation == HORIZONTAL)
			{
				new_height = getDefaultHeight(new_height);
			}
			else // VERTICAL
			{
				new_width = getDefaultWidth(new_width);
			}
		}

		// adjust running headroom count based on new sizes
		shrink_headroom_total += delta_size;

		LLRect panel_rect;
		panel_rect.setLeftTopAndSize(cur_x, cur_y, new_width, new_height);
		panelp->setShape(panel_rect);

		LLRect resize_bar_rect = panel_rect;
		if (mOrientation == HORIZONTAL)
		{
			resize_bar_rect.mLeft = panel_rect.mRight - resize_bar_overlap;
			resize_bar_rect.mRight = panel_rect.mRight + mPanelSpacing + resize_bar_overlap;
		}
		else
		{
			resize_bar_rect.mTop = panel_rect.mBottom + resize_bar_overlap;
			resize_bar_rect.mBottom = panel_rect.mBottom - mPanelSpacing - resize_bar_overlap;
		}
		(*panel_it)->mResizeBar->setRect(resize_bar_rect);

		if (mOrientation == HORIZONTAL)
		{
			cur_x += llround(new_width * (*panel_it)->getCollapseFactor(mOrientation)) + mPanelSpacing;
		}
		else //VERTICAL
		{
			cur_y -= llround(new_height * (*panel_it)->getCollapseFactor(mOrientation)) + mPanelSpacing;
		}
	}

	// update resize bars with new limits
	LLResizeBar* last_resize_bar = NULL;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLPanel* panelp = (*panel_it);

		if (mOrientation == HORIZONTAL)
		{
			(*panel_it)->mResizeBar->setResizeLimits(
				(*panel_it)->mMinDim, 
				(*panel_it)->mMinDim + shrink_headroom_total);
		}
		else //VERTICAL
		{
			(*panel_it)->mResizeBar->setResizeLimits(
				(*panel_it)->mMinDim, 
				(*panel_it)->mMinDim + shrink_headroom_total);
		}

		// toggle resize bars based on panel visibility, resizability, etc
		BOOL resize_bar_enabled = panelp->getVisible() && (*panel_it)->mUserResize;
		(*panel_it)->mResizeBar->setVisible(resize_bar_enabled);

		if (resize_bar_enabled)
		{
			last_resize_bar = (*panel_it)->mResizeBar;
		}
	}

	// hide last resize bar as there is nothing past it
	// resize bars need to be in between two resizable panels
	if (last_resize_bar)
	{
		last_resize_bar->setVisible(FALSE);
	}

	// not enough room to fit existing contents
	if (force_resize == FALSE
		// layout did not complete by reaching target position
		&& ((mOrientation == VERTICAL && cur_y != -mPanelSpacing)
			|| (mOrientation == HORIZONTAL && cur_x != getRect().getWidth() + mPanelSpacing)))
	{
		// do another layout pass with all stacked elements contributing
		// even those that don't usually resize
		llassert_always(force_resize == FALSE);
		updateLayout(TRUE);
	}

	 mAnimatedThisFrame = true;
} // end LLLayoutStack::updateLayout


LLLayoutPanel* LLLayoutStack::findEmbeddedPanel(LLPanel* panelp) const
{
	if (!panelp) return NULL;

	e_panel_list_t::const_iterator panel_it;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		if ((*panel_it) == panelp)
		{
			return *panel_it;
		}
	}
	return NULL;
}

LLLayoutPanel* LLLayoutStack::findEmbeddedPanelByName(const std::string& name) const
{
	LLLayoutPanel* result = NULL;

	for (e_panel_list_t::const_iterator panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLLayoutPanel* p = *panel_it;

		if (p->getName() == name)
		{
			result = p;
			break;
		}
	}

	return result;
}

// Compute sum of min_width or min_height of children
void LLLayoutStack::calcMinExtents()
{
	mMinWidth = 0;
	mMinHeight = 0;

	e_panel_list_t::iterator panel_it;
	for (panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		if (mOrientation == HORIZONTAL)
		{
            mMinWidth += (*panel_it)->mMinDim;
			if (panel_it != mPanels.begin())
			{
				mMinWidth += mPanelSpacing;
			}
		}
		else //VERTICAL
		{
			mMinHeight += (*panel_it)->mMinDim;
			if (panel_it != mPanels.begin())
			{
				mMinHeight += mPanelSpacing;
			}
		}
	}
}

void LLLayoutStack::createResizeBars()
{
	for (e_panel_list_t::iterator panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLLayoutPanel* lp = (*panel_it);
		if (lp->mResizeBar == NULL)
		{
			LLResizeBar::Side side = (mOrientation == HORIZONTAL) ? LLResizeBar::RIGHT : LLResizeBar::BOTTOM;
			LLRect resize_bar_rect = getRect();

			LLResizeBar::Params resize_params;
			resize_params.name("resize");
			resize_params.resizing_view(this);
			resize_params.min_size(lp->mMinDim);
			resize_params.side(side);
			resize_params.snapping_enabled(false);
			LLResizeBar* resize_bar = LLUICtrlFactory::create<LLResizeBar>(resize_params);
			lp->mResizeBar = resize_bar;
			LLView::addChild(resize_bar, 0);

			// bring all resize bars to the front so that they are clickable even over the panels
			// with a bit of overlap
			for (e_panel_list_t::iterator panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
			{
				LLResizeBar* resize_barp = (*panel_it)->mResizeBar;
				sendChildToFront(resize_barp);
			}
		}
	}
}

// update layout stack animations, etc. once per frame
// NOTE: we use this to size world view based on animating UI, *before* we draw the UI
// we might still need to call updateLayout during UI draw phase, in case UI elements
// are resizing themselves dynamically
//static 
void LLLayoutStack::updateClass()
{
	LLInstanceTrackerScopedGuard guard;
	for (LLLayoutStack::instance_iter it = guard.beginInstances();
	     it != guard.endInstances();
	     ++it)
	{
		it->updateLayout();
	}
}
