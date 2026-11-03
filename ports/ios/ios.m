/**
 *    Name:        ios.m
 *    Description: what iOS does its own way: the folder the Files app shows, and the ROM set chosen there
 *    Version:     0.1.0
 *    Created:     03-11-2026
 *    Modified:    03-11-2026
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 */
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "ios.h"

#include <SDL_syswm.h>

#include <objc/message.h>
#include <objc/runtime.h>

/* The system's scene, which the SDK's apps must show themselves in (Info.plist.in names the delegate). SDL 2 knows
   none: its window goes in it when it is made (window_in_scene), and the lifecycle's events reach it as the
   UIApplication notifications it already watches */
static UIWindowScene *scene;

@interface GameScene : UIResponder <UIWindowSceneDelegate>
@end

@implementation GameScene

- (void)scene:(UIScene *)connected willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options
{
	scene = (UIWindowScene *)connected;
}

@end

/* SDL's window's initWithFrame:, the only window it makes without a launch storyboard */
static id window_in_scene(id self, SEL selector, CGRect frame)
{
	struct objc_super parent = { self, [UIWindow class] };

	self = ((id (*)(struct objc_super *, SEL, CGRect))objc_msgSendSuper)(&parent, selector, frame);
	((UIWindow *)self).windowScene = scene;
	return self;
}

void ios_wait_for_scene(void)
{
	while (!scene)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	class_addMethod(objc_getClass("SDL_uikitwindow"), @selector(initWithFrame:), (IMP)window_in_scene,
		method_getTypeEncoding(class_getInstanceMethod([UIWindow class], @selector(initWithFrame:))));
}

@interface RomSetPicker : NSObject <UIDocumentPickerDelegate>
@end

@implementation RomSetPicker

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
	SDL_Event event;

	if (urls.count == 0)
		return;
	SDL_zero(event);
	event.type = SDL_DROPFILE;
	event.drop.file = SDL_strdup(urls.firstObject.path.fileSystemRepresentation);
	SDL_PushEvent(&event);
}

@end

char *ios_documents_folder(void)
{
	NSString *folder = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;

	return folder ? SDL_strdup([folder stringByAppendingString:@"/"].fileSystemRepresentation) : NULL;
}

void ios_pick_rom_set(SDL_Window *window)
{
	static RomSetPicker *delegate;    /* (the picker holds its delegate weakly) */
	SDL_SysWMinfo info;
	UIViewController *root;
	UIDocumentPickerViewController *picker;

	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info) || !(root = info.info.uikit.window.rootViewController)
		|| root.presentedViewController)
		return;
	if (!delegate)
		delegate = [RomSetPicker new];
	picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeZIP ] asCopy:YES];
	picker.delegate = delegate;
	[root presentViewController:picker animated:YES completion:nil];
}
