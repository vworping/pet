CREATE TABLE `assignments` (
	`event_id` text PRIMARY KEY NOT NULL,
	`card_id` text NOT NULL,
	`encounter_id` text NOT NULL,
	`assigned_at` integer NOT NULL,
	FOREIGN KEY (`card_id`) REFERENCES `cards`(`id`) ON UPDATE no action ON DELETE no action,
	FOREIGN KEY (`encounter_id`) REFERENCES `encounters`(`id`) ON UPDATE no action ON DELETE no action
);
--> statement-breakpoint
CREATE UNIQUE INDEX `assignments_card_id_unique` ON `assignments` (`card_id`);--> statement-breakpoint
CREATE UNIQUE INDEX `assignments_encounter_id_unique` ON `assignments` (`encounter_id`);--> statement-breakpoint
CREATE TABLE `cards` (
	`id` text PRIMARY KEY NOT NULL,
	`label` text DEFAULT 'Andre Weiss' NOT NULL,
	`note` text DEFAULT 'Photographer × researcher' NOT NULL,
	`url` text DEFAULT 'https://mindofaxd.com' NOT NULL,
	`connection_note` text DEFAULT '' NOT NULL,
	`revision` integer DEFAULT 1 NOT NULL,
	`created_at` integer NOT NULL,
	`disabled` integer DEFAULT 0 NOT NULL
);
--> statement-breakpoint
CREATE TABLE `encounters` (
	`id` text PRIMARY KEY NOT NULL,
	`created_at` integer NOT NULL,
	`expires_at` integer NOT NULL,
	`opened_at` integer,
	`submitted_at` integer,
	`name` text,
	`topic` text,
	`share_topic` integer DEFAULT 0 NOT NULL,
	`consent_version` text,
	`closed` integer DEFAULT 0 NOT NULL
);
--> statement-breakpoint
CREATE INDEX `idx_encounters_created` ON `encounters` (`created_at`);