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
#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "ios.h"

#include <SDL_syswm.h>

#include <math.h>
#include <objc/message.h>
#include <objc/runtime.h>

#define TILT_FULL       (30.0 * M_PI / 180.0)    /* the wheel all the way, in the phone's turn */
#define TILT_DEAD       (1.5 * M_PI / 180.0)     /* a turn this small, the wheel at the centre */
#define TILT_FLAT       0.2                      /* gravity across the screen, in g: less, the phone lies flat */
#define TILT_TURNED     (135.0 * M_PI / 180.0)   /* turned this far, the other way up: centred again */

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

static CMMotionManager *motion;
static double centre_x, centre_y;    /* gravity across the screen where the wheel is centred; both 0: not yet */

bool ios_tilt_available(void)
{
	if (!motion)
		motion = [CMMotionManager new];
	return motion.deviceMotionAvailable;
}

void ios_tilt_start(void)
{
	centre_x = centre_y = 0.0;
	if (!ios_tilt_available())
		return;
	motion.deviceMotionUpdateInterval = 1.0 / 60.0;
	[motion startDeviceMotionUpdates];
}

void ios_tilt_stop(void)
{
	[motion stopDeviceMotionUpdates];
}

double ios_tilt(void)
{
	CMDeviceMotion *now = motion.deviceMotion;
	double x, y, turn;

	if (!now || hypot(now.gravity.x, now.gravity.y) < TILT_FLAT)
		return 0.0;
	x = now.gravity.x;
	y = now.gravity.y;
	if (centre_x == 0.0 && centre_y == 0.0)
	{
		centre_x = x;
		centre_y = y;
	}
	/* (the phone turned clockwise, as its player sees the screen, turns gravity the other way across it) */
	turn = atan2(centre_x * y - centre_y * x, centre_x * x + centre_y * y);
	if (fabs(turn) > TILT_TURNED)
	{
		centre_x = x;
		centre_y = y;
		return 0.0;
	}
	if (fabs(turn) < TILT_DEAD)
		return 0.0;
	return fmax(-1.0, fmin(1.0, turn / TILT_FULL));
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
