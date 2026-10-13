import { sqliteTable, text, integer, index } from 'drizzle-orm/sqlite-core';
export const encounters = sqliteTable('encounters', {
 id: text('id').primaryKey(), created_at: integer('created_at').notNull(), expires_at: integer('expires_at').notNull(),
 opened_at: integer('opened_at'), submitted_at: integer('submitted_at'), name: text('name'), topic: text('topic'),
 share_topic: integer('share_topic').notNull().default(0), share_name: integer('share_name').notNull().default(0), consent_version: text('consent_version'),
 closed: integer('closed').notNull().default(0)
}, t => [index('idx_encounters_created').on(t.created_at)]);
export const cards = sqliteTable('cards', {
 id: text('id').primaryKey(), label: text('label').notNull().default('Andre Weiss'),
 note: text('note').notNull().default('Photographer × researcher'), url: text('url').notNull().default('https://mindofaxd.com'),
 connection_note: text('connection_note').notNull().default(''), revision: integer('revision').notNull().default(1),
 created_at: integer('created_at').notNull(), disabled: integer('disabled').notNull().default(0)
});
export const assignments = sqliteTable('assignments', {
 event_id: text('event_id').primaryKey(), card_id: text('card_id').notNull().unique().references(()=>cards.id),
 encounter_id: text('encounter_id').notNull().unique().references(()=>encounters.id), assigned_at: integer('assigned_at').notNull()
});
