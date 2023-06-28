// This file Copyright © 2006-2023 Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import "TorrentCellRevealButton.h"
#import "TorrentTableView.h"

@interface TorrentCellRevealButton ()
@property(nonatomic) NSTrackingArea* fTrackingArea;
@property(nonatomic, copy) NSString* revealImageString;
@property(nonatomic) TorrentTableView* torrentTableView;
@end

@implementation TorrentCellRevealButton

- (void)awakeFromNib
{
    self.revealImageString = @"RevealOff";
    [self updateImage];
}

- (void)setupTorrentTableView
{
    if (!self.torrentTableView)
    {
        self.torrentTableView = (TorrentTableView*)[[[self superview] superview] superview];
    }
}

- (void)mouseEntered:(NSEvent*)event
{
    [super mouseEntered:event];
    self.revealImageString = @"RevealHover";
    [self updateImage];

    [self.torrentTableView hoverEventBeganForView:self];
}

- (void)mouseExited:(NSEvent*)event
{
    [super mouseExited:event];
    self.revealImageString = @"RevealOff";
    [self updateImage];

    [self.torrentTableView hoverEventEndedForView:self];
}

- (void)mouseDown:(NSEvent*)event
{
    //when filterbar is shown, we need to remove focus otherwise action fails
    [self.window makeFirstResponder:self.torrentTableView];

    [super mouseDown:event];
    self.revealImageString = @"RevealOn";
    [self updateImage];
}

- (void)updateImage
{
    [self setupTorrentTableView];

    NSImage* revealImage = [NSImage imageNamed:self.revealImageString];
    self.image = revealImage;
    [self setNeedsDisplay:YES];
}

- (void)updateTrackingAreas
{
    if (self.fTrackingArea != nil)
    {
        [self removeTrackingArea:self.fTrackingArea];
    }

    NSTrackingAreaOptions opts = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    self.fTrackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds options:opts owner:self userInfo:nil];
    [self addTrackingArea:self.fTrackingArea];
}

@end
