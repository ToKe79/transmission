/******************************************************************************
 * Copyright (c) 2010-2012 Transmission authors and contributors
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#import "InfoGeneralViewController.h"
#import "NSStringAdditions.h"
#import "Torrent.h"

@interface InfoGeneralViewController (Private)

- (void)setupInfo;

@end

@implementation InfoGeneralViewController

- (instancetype)init
{
    if ((self = [super initWithNibName:@"InfoGeneralView" bundle:nil]))
    {
        self.title = NSLocalizedString(@"General Info", "Inspector view -> title");
    }

    return self;
}

- (void)awakeFromNib
{
#warning remove when 10.7-only with auto layout
    [fInfoSectionLabel sizeToFit];
    [fWhereSectionLabel sizeToFit];

    NSArray* labels = @[
        fPiecesLabel,
        fHashLabel,
        fSecureLabel,
        fCreatorLabel,
        fDateCreatedLabel,
        fCommentLabel,
        fDataLocationLabel,
    ];

    CGFloat oldMaxWidth = 0.0, originX, newMaxWidth = 0.0;
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
        fPiecesField,
        fHashField,
        fSecureField,
        fCreatorField,
        fDateCreatedField,
        fCommentScrollView,
        fDataLocationField,
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

- (void)setInfoForTorrents:(NSArray*)torrents
{
    //don't check if it's the same in case the metadata changed
    fTorrents = torrents;

    fSet = NO;
}

- (void)updateInfo
{
    if (!fSet)
    {
        [self setupInfo];
    }

    if (fTorrents.count != 1)
    {
        return;
    }

    Torrent* torrent = fTorrents[0];

    NSString* location = torrent.dataLocation;
    fDataLocationField.stringValue = location ? location.stringByAbbreviatingWithTildeInPath : @"";
    fDataLocationField.toolTip = location ? location : @"";

    fRevealDataButton.hidden = !location;
}

- (void)revealDataFile:(id)sender
{
    Torrent* torrent = fTorrents[0];
    NSString* location = torrent.dataLocation;
    if (!location)
    {
        return;
    }

    NSURL* file = [NSURL fileURLWithPath:location];
    [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ file ]];
}

@end

@implementation InfoGeneralViewController (Private)

- (void)setupInfo
{
    if (fTorrents.count == 1)
    {
        Torrent* torrent = fTorrents[0];

#warning candidate for localizedStringWithFormat (although then we'll get two commas)
        NSString* piecesString = !torrent.magnet ?
            [NSString stringWithFormat:@"%ld, %@", torrent.pieceCount, [NSString stringForFileSize:torrent.pieceSize]] :
            @"";
        fPiecesField.stringValue = piecesString;

        NSString* hashString = torrent.hashString;
        fHashField.stringValue = hashString;
        fHashField.toolTip = hashString;
        fSecureField.stringValue = torrent.privateTorrent ?
            NSLocalizedString(@"Private Torrent, non-tracker peer discovery disabled", "Inspector -> private torrent") :
            NSLocalizedString(@"Public Torrent", "Inspector -> private torrent");

        NSString* commentString = torrent.comment;
        fCommentView.string = commentString;

        NSString* creatorString = torrent.creator;
        fCreatorField.stringValue = creatorString;
        fDateCreatedField.objectValue = torrent.dateCreated;
    }
    else
    {
        fPiecesField.stringValue = @"";
        fHashField.stringValue = @"";
        fHashField.toolTip = nil;
        fSecureField.stringValue = @"";
        fCommentView.string = @"";

        fCreatorField.stringValue = @"";
        fDateCreatedField.stringValue = @"";

        fDataLocationField.stringValue = @"";
        fDataLocationField.toolTip = nil;

        fRevealDataButton.hidden = YES;
    }

    fSet = YES;
}

@end
