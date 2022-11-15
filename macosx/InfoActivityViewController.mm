// This file Copyright © 2010-2022 Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> //tr_getRatio()

#import "InfoActivityViewController.h"
#import "NSStringAdditions.h"
#import "PiecesView.h"
#import "Torrent.h"

typedef NS_ENUM(NSUInteger, PiecesControlSegment) {
    PiecesControlSegmentProgress = 0,
    PiecesControlSegmentAvailable = 1,
};

static CGFloat const kStackViewInset = 12.0;
static CGFloat const kStackViewHorizontalSpacing = 20.0;
static CGFloat const kStackViewVerticalSpacing = 8.0;

@interface InfoActivityViewController ()

@property(nonatomic, copy) NSArray<Torrent*>* fTorrents;

@property(nonatomic) BOOL fSet;

@property(nonatomic) IBOutlet NSTextField* fDateAddedField;
@property(nonatomic) IBOutlet NSTextField* fDateCompletedField;
@property(nonatomic) IBOutlet NSTextField* fDateActivityField;
@property(nonatomic) IBOutlet NSTextField* fStateField;
@property(nonatomic) IBOutlet NSTextField* fProgressField;
@property(nonatomic) IBOutlet NSTextField* fHaveField;
@property(nonatomic) IBOutlet NSTextField* fDownloadedTotalField;
@property(nonatomic) IBOutlet NSTextField* fUploadedTotalField;
@property(nonatomic) IBOutlet NSTextField* fFailedHashField;
@property(nonatomic) IBOutlet NSTextField* fRatioField;
@property(nonatomic) IBOutlet NSTextField* fDownloadTimeField;
@property(nonatomic) IBOutlet NSTextField* fSeedTimeField;
@property(nonatomic) IBOutlet NSTextView* fErrorMessageView;

@property(nonatomic) IBOutlet PiecesView* fPiecesView;
@property(nonatomic) IBOutlet NSSegmentedControl* fPiecesControl;

//remove when we switch to auto layout
@property(nonatomic) IBOutlet NSTextField* fTransferSectionLabel;
@property(nonatomic) IBOutlet NSTextField* fDatesSectionLabel;
@property(nonatomic) IBOutlet NSTextField* fTimeSectionLabel;
@property(nonatomic) IBOutlet NSTextField* fStateLabel;
@property(nonatomic) IBOutlet NSTextField* fProgressLabel;
@property(nonatomic) IBOutlet NSTextField* fHaveLabel;
@property(nonatomic) IBOutlet NSTextField* fDownloadedLabel;
@property(nonatomic) IBOutlet NSTextField* fUploadedLabel;
@property(nonatomic) IBOutlet NSTextField* fFailedDLLabel;
@property(nonatomic) IBOutlet NSTextField* fRatioLabel;
@property(nonatomic) IBOutlet NSTextField* fErrorLabel;
@property(nonatomic) IBOutlet NSTextField* fDateAddedLabel;
@property(nonatomic) IBOutlet NSTextField* fDateCompletedLabel;
@property(nonatomic) IBOutlet NSTextField* fDateActivityLabel;
@property(nonatomic) IBOutlet NSTextField* fDownloadTimeLabel;
@property(nonatomic) IBOutlet NSTextField* fSeedTimeLabel;
@property(nonatomic) IBOutlet NSScrollView* fErrorScrollView;

@property(nonatomic) IBOutlet NSStackView* fActivityStackView;
@property(nonatomic) IBOutlet NSView* fDatesView;
@property(nonatomic, readonly) CGFloat currentHeight;
@property(nonatomic, readonly) CGFloat horizLayoutHeight;
@property(nonatomic, readonly) CGFloat horizLayoutWidth;
@property(nonatomic, readonly) CGFloat vertLayoutHeight;

@end

@implementation InfoActivityViewController

- (instancetype)init
{
    if ((self = [super initWithNibName:@"InfoActivityView" bundle:nil]))
    {
        self.title = NSLocalizedString(@"Activity", "Inspector view -> title");
    }

    return self;
}

- (void)awakeFromNib
{
    [self.fTransferSectionLabel sizeToFit];
    [self.fDatesSectionLabel sizeToFit];
    [self.fTimeSectionLabel sizeToFit];

    NSArray* labels = @[
        self.fStateLabel,
        self.fProgressLabel,
        self.fHaveLabel,
        self.fDownloadedLabel,
        self.fUploadedLabel,
        self.fFailedDLLabel,
        self.fRatioLabel,
        self.fErrorLabel,
        self.fDateAddedLabel,
        self.fDateCompletedLabel,
        self.fDateActivityLabel,
        self.fDownloadTimeLabel,
        self.fSeedTimeLabel
    ];

    CGFloat oldMaxWidth = 0.0, originX = 0.0, newMaxWidth = 0.0;
    for (NSTextField* label in labels)
    {
        NSRect const oldFrame = label.frame;
        if (oldFrame.size.width > oldMaxWidth)
        {
            oldMaxWidth = oldFrame.size.width;
            originX = oldFrame.origin.x;
        }

        [label sizeToFit];
        CGFloat const newWidth = label.bounds.size.width;
        if (newWidth > newMaxWidth)
        {
            newMaxWidth = newWidth;
        }
    }

    for (NSTextField* label in labels)
    {
        NSRect frame = label.frame;
        frame.origin.x = originX + (newMaxWidth - frame.size.width);
        label.frame = frame;
    }

    NSArray* fields = @[
        self.fDateAddedField,
        self.fDateCompletedField,
        self.fDateActivityField,
        self.fStateField,
        self.fProgressField,
        self.fHaveField,
        self.fDownloadedTotalField,
        self.fUploadedTotalField,
        self.fFailedHashField,
        self.fRatioField,
        self.fDownloadTimeField,
        self.fSeedTimeField,
        self.fErrorScrollView
    ];

    CGFloat const widthIncrease = newMaxWidth - oldMaxWidth;
    for (NSView* field in fields)
    {
        NSRect frame = field.frame;
        frame.origin.x += widthIncrease;
        frame.size.width -= widthIncrease;
        field.frame = frame;
    }
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (CGFloat)currentHeight
{
    return NSHeight(self.view.frame);
}

- (CGFloat)horizLayoutHeight
{
    return NSHeight(self.fTransferView.frame) + 2 * kStackViewInset;
}

- (CGFloat)horizLayoutWidth
{
    return NSWidth(self.fTransferView.frame) + NSWidth(self.fDatesView.frame) + (2 * kStackViewInset) + kStackViewHorizontalSpacing;
}

- (CGFloat)vertLayoutHeight
{
    return NSHeight(self.fTransferView.frame) + NSHeight(self.fDatesView.frame) + (2 * kStackViewInset) + kStackViewVerticalSpacing;
}

- (CGFloat)changeInWindowHeight
{
    CGFloat difference = 0;

    if (NSWidth(self.view.window.frame) >= self.horizLayoutWidth + 1)
    {
        self.fActivityStackView.orientation = NSUserInterfaceLayoutOrientationHorizontal;

        //add some padding between views in horizontal layout
        self.fActivityStackView.spacing = kStackViewHorizontalSpacing;

        difference = NSHeight(self.view.frame) - self.horizLayoutHeight;
    }
    else
    {
        self.fActivityStackView.orientation = NSUserInterfaceLayoutOrientationVertical;
        self.fActivityStackView.spacing = kStackViewVerticalSpacing;

        difference = NSHeight(self.view.frame) - self.vertLayoutHeight;
    }

    return difference;
}

- (NSRect)viewRect
{
    CGFloat difference = self.changeInWindowHeight;

    NSRect windowRect = self.view.window.frame, viewRect = self.view.frame;
    if (difference != 0)
    {
        viewRect.size.height -= difference;
        viewRect.size.width = NSWidth(windowRect);
    }

    return viewRect;
}

- (void)updateWindowLayout
{
    CGFloat difference = self.changeInWindowHeight;

    if (difference != 0)
    {
        NSRect windowRect = self.view.window.frame;
        windowRect.origin.y += difference;
        windowRect.size.height -= difference;

        self.view.window.minSize = NSMakeSize(self.view.window.minSize.width, NSHeight(windowRect));
        self.view.window.maxSize = NSMakeSize(FLT_MAX, NSHeight(windowRect));

        self.view.frame = [self viewRect];
        [self.view.window setFrame:windowRect display:YES animate:YES];
    }
}

- (void)setInfoForTorrents:(NSArray<Torrent*>*)torrents
{
    //don't check if it's the same in case the metadata changed
    self.fTorrents = torrents;

    self.fSet = NO;
}

- (void)updateInfo
{
    if (!self.fSet)
    {
        [self setupInfo];
    }

    NSInteger const numberSelected = self.fTorrents.count;
    if (numberSelected == 0)
    {
        return;
    }

    uint64_t have = 0, haveVerified = 0, downloadedTotal = 0, uploadedTotal = 0, failedHash = 0;
    NSDate* lastActivity = nil;
    for (Torrent* torrent in self.fTorrents)
    {
        have += torrent.haveTotal;
        haveVerified += torrent.haveVerified;
        downloadedTotal += torrent.downloadedTotal;
        uploadedTotal += torrent.uploadedTotal;
        failedHash += torrent.failedHash;

        NSDate* nextLastActivity;
        if ((nextLastActivity = torrent.dateActivity))
        {
            lastActivity = lastActivity ? [lastActivity laterDate:nextLastActivity] : nextLastActivity;
        }
    }

    if (have == 0)
    {
        self.fHaveField.stringValue = [NSString stringForFileSize:0];
    }
    else
    {
        NSString* verifiedString = [NSString stringWithFormat:NSLocalizedString(@"%@ verified", "Inspector -> Activity tab -> have"),
                                                              [NSString stringForFileSize:haveVerified]];
        if (have == haveVerified)
        {
            self.fHaveField.stringValue = verifiedString;
        }
        else
        {
            self.fHaveField.stringValue = [NSString stringWithFormat:@"%@ (%@)", [NSString stringForFileSize:have], verifiedString];
        }
    }

    self.fDownloadedTotalField.stringValue = [NSString stringForFileSize:downloadedTotal];
    self.fUploadedTotalField.stringValue = [NSString stringForFileSize:uploadedTotal];
    self.fFailedHashField.stringValue = [NSString stringForFileSize:failedHash];

    self.fDateActivityField.objectValue = lastActivity;

    if (numberSelected == 1)
    {
        Torrent* torrent = self.fTorrents[0];

        self.fStateField.stringValue = torrent.stateString;

        NSString* progressString = [NSString percentString:torrent.progress longDecimals:YES];
        if (torrent.folder)
        {
            NSString* progressSelectedString = [NSString
                stringWithFormat:NSLocalizedString(@"%@ selected", "Inspector -> Activity tab -> progress"),
                                 [NSString percentString:torrent.progressDone longDecimals:YES]];
            progressString = [progressString stringByAppendingFormat:@" (%@)", progressSelectedString];
        }
        self.fProgressField.stringValue = progressString;

        self.fRatioField.stringValue = [NSString stringForRatio:torrent.ratio];

        NSString* errorMessage = torrent.errorMessage;
        if (![errorMessage isEqualToString:self.fErrorMessageView.string])
            self.fErrorMessageView.string = errorMessage;

        self.fDateCompletedField.objectValue = torrent.dateCompleted;

        //uses a relative date, so can't be set once
        self.fDateAddedField.objectValue = torrent.dateAdded;

        static NSDateComponentsFormatter* timeFormatter;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            timeFormatter = [NSDateComponentsFormatter new];
            timeFormatter.unitsStyle = NSDateComponentsFormatterUnitsStyleShort;
            timeFormatter.allowedUnits = NSCalendarUnitDay | NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
            timeFormatter.zeroFormattingBehavior = NSDateComponentsFormatterZeroFormattingBehaviorDropLeading;
        });

        self.fDownloadTimeField.stringValue = [timeFormatter stringFromTimeInterval:torrent.secondsDownloading];
        self.fSeedTimeField.stringValue = [timeFormatter stringFromTimeInterval:torrent.secondsSeeding];

        [self.fPiecesView updateView];
    }
    else if (numberSelected > 1)
    {
        self.fRatioField.stringValue = [NSString stringForRatio:tr_getRatio(uploadedTotal, downloadedTotal)];
    }
}

- (void)setPiecesView:(id)sender
{
    BOOL const availability = [sender selectedSegment] == PiecesControlSegmentAvailable;
    [NSUserDefaults.standardUserDefaults setBool:availability forKey:@"PiecesViewShowAvailability"];
    [self updatePiecesView:nil];
}

- (void)updatePiecesView:(id)sender
{
    BOOL const piecesAvailableSegment = [NSUserDefaults.standardUserDefaults boolForKey:@"PiecesViewShowAvailability"];

    [self.fPiecesControl setSelected:piecesAvailableSegment forSegment:PiecesControlSegmentAvailable];
    [self.fPiecesControl setSelected:!piecesAvailableSegment forSegment:PiecesControlSegmentProgress];

    [self.fPiecesView updateView];
}

- (void)clearView
{
    [self.fPiecesView clearView];
}

#pragma mark - Private

- (void)setupInfo
{
    NSUInteger const count = self.fTorrents.count;
    if (count != 1)
    {
        if (count == 0)
        {
            self.fHaveField.stringValue = @"";
            self.fDownloadedTotalField.stringValue = @"";
            self.fUploadedTotalField.stringValue = @"";
            self.fFailedHashField.stringValue = @"";
            self.fDateActivityField.objectValue = @""; //using [field setStringValue: @""] causes "December 31, 1969 7:00 PM" to be displayed, at least on 10.7.3
            self.fRatioField.stringValue = @"";
        }

        self.fStateField.stringValue = @"";
        self.fProgressField.stringValue = @"";

        self.fErrorMessageView.string = @"";

        //using [field setStringValue: @""] causes "December 31, 1969 7:00 PM" to be displayed, at least on 10.7.3
        self.fDateAddedField.objectValue = @"";
        self.fDateCompletedField.objectValue = @"";

        self.fDownloadTimeField.stringValue = @"";
        self.fSeedTimeField.stringValue = @"";

        [self.fPiecesControl setSelected:NO forSegment:PiecesControlSegmentAvailable];
        [self.fPiecesControl setSelected:NO forSegment:PiecesControlSegmentProgress];
        self.fPiecesControl.enabled = NO;
        self.fPiecesView.torrent = nil;
    }
    else
    {
        Torrent* torrent = self.fTorrents[0];

        BOOL const piecesAvailableSegment = [NSUserDefaults.standardUserDefaults boolForKey:@"PiecesViewShowAvailability"];
        [self.fPiecesControl setSelected:piecesAvailableSegment forSegment:PiecesControlSegmentAvailable];
        [self.fPiecesControl setSelected:!piecesAvailableSegment forSegment:PiecesControlSegmentProgress];
        self.fPiecesControl.enabled = YES;

        self.fPiecesView.torrent = torrent;
    }

    self.fSet = YES;
}

@end
